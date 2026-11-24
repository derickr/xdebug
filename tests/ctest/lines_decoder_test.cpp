#include "CppUTest/TestHarness.h"

#include <unistd.h>

#include "lines-encoder.h"

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)
#define LOCATION __FILE__ ":" STRINGIZE(__LINE__)

TEST_GROUP(lines_decoder)
{
	bool result;

	TEST_SETUP()
	{
	}

	bool test_decode_line(const char *remote_string, const char *local_string)
	{
		return xdebug_lines_decoder(
			remote_string,
			local_string
		);
	}

	TEST_TEARDOWN()
	{
	}

private:
	void reset_result()
	{
	}
};

TEST(lines_decoder, local_range_empty_start)
{
	const char *remote_line = "ChNBBBCRRBBBFvPBRBC";
	const char *local_line = "RRRRRRR_kDBR";

	result = test_decode_line(remote_line, local_line);
}
