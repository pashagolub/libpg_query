#include <pg_query.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

static bool
test_location_info(void)
{
	/*
	 * Test that pg_query_parse_plpgsql() includes location (byte offset)
	 * information in its JSON output, similar to pg_query_parse().
	 *
	 * The function body starts right after the opening $$, so byte offsets
	 * are relative to the start of the function body string.
	 */
	const char *func_sql =
		"CREATE OR REPLACE FUNCTION location_test(x int) RETURNS int AS $$\n"
		"BEGIN\n"
		"  IF x > 0 THEN\n"
		"    RETURN x;\n"
		"  END IF;\n"
		"  RETURN 0;\n"
		"END;\n"
		"$$ LANGUAGE plpgsql;";

	PgQueryPlpgsqlParseResult result = pg_query_parse_plpgsql(func_sql);

	if (result.error) {
		printf("LOCATION TEST ERROR: %s\n", result.error->message);
		pg_query_free_plpgsql_parse_result(result);
		return false;
	}

	/* Verify that 'location' fields are present in the JSON output */
	if (strstr(result.plpgsql_funcs, "\"location\":") == NULL) {
		printf("LOCATION TEST FAILED: no 'location' fields found in output\n");
		printf("Output: %s\n", result.plpgsql_funcs);
		pg_query_free_plpgsql_parse_result(result);
		return false;
	}

	/* Verify that PLpgSQL_stmt_block has a location */
	if (strstr(result.plpgsql_funcs, "\"PLpgSQL_stmt_block\":{\"lineno\":2,\"location\":") == NULL) {
		printf("LOCATION TEST FAILED: PLpgSQL_stmt_block missing location\n");
		printf("Output: %s\n", result.plpgsql_funcs);
		pg_query_free_plpgsql_parse_result(result);
		return false;
	}

	/* Verify that PLpgSQL_stmt_if has a location */
	if (strstr(result.plpgsql_funcs, "\"PLpgSQL_stmt_if\":{\"lineno\":3,\"location\":") == NULL) {
		printf("LOCATION TEST FAILED: PLpgSQL_stmt_if missing location\n");
		printf("Output: %s\n", result.plpgsql_funcs);
		pg_query_free_plpgsql_parse_result(result);
		return false;
	}

	/* Verify that PLpgSQL_stmt_return has a location (lineno=4, inside the IF block) */
	if (strstr(result.plpgsql_funcs, "\"PLpgSQL_stmt_return\":{\"lineno\":4,\"location\":") == NULL) {
		printf("LOCATION TEST FAILED: PLpgSQL_stmt_return missing location\n");
		printf("Output: %s\n", result.plpgsql_funcs);
		pg_query_free_plpgsql_parse_result(result);
		return false;
	}

	pg_query_free_plpgsql_parse_result(result);
	return true;
}

int main() {
	bool ret_code = EXIT_SUCCESS;
	char *sample_buffer;
	struct stat sample_stat;
	int fd;
	FILE* f_out;
	PgQueryPlpgsqlParseResult result;

	fd = open("test/plpgsql_samples.sql", O_RDONLY);
	if (fd < 0) {
		printf("Could not read samples file\n");
		return EXIT_FAILURE;
    }

	fstat(fd, &sample_stat);

	sample_buffer = malloc(sample_stat.st_size + 1);
	read(fd, sample_buffer, sample_stat.st_size);
	sample_buffer[sample_stat.st_size] = 0;

	if (sample_buffer != (void *) - 1)
	{
		result = pg_query_parse_plpgsql(sample_buffer);
		free(sample_buffer);
		close(fd);
	} else {
		printf("Could not read samples file\n");
		close(fd);
		return EXIT_FAILURE;
	}

	if (result.error) {
		printf("ERROR: %s\n", result.error->message);
		printf("CONTEXT: %s\n", result.error->context);
		printf("LOCATION: %s, %s:%d\n\n", result.error->funcname, result.error->filename, result.error->lineno);

		pg_query_free_plpgsql_parse_result(result);
		return EXIT_FAILURE;
	}

	f_out = fopen("test/plpgsql_samples.actual.json", "w");
	fprintf(f_out, "%s\n", result.plpgsql_funcs);
	fclose(f_out);

	pg_query_free_plpgsql_parse_result(result);

	/* Run location information tests */
	if (!test_location_info()) {
		ret_code = EXIT_FAILURE;
	} else {
		printf("Location info tests: PASSED\n");
	}

	pg_query_exit();

	return ret_code;
}
