TEST_DIR=test
SCRIPT_DIR=scripts
TEST_SCRIPT_NAME=run_test.sh
GEN_DOC_SCRIPT_NAME=generate_doc.sh

.PHONY: test doc

test:
	cd ./$(TEST_DIR) && bash $(TEST_SCRIPT_NAME)

doc:
	cd ./$(SCRIPT_DIR) && bash $(GEN_DOC_SCRIPT_NAME)
