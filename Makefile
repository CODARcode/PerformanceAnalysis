TEST_DIR=test
TEST_SCRIPT_NAME=run_test.sh

.PHONY: test

test:
	cd ./$(TEST_DIR) && bash $(TEST_SCRIPT_NAME)
