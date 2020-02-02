#include <cstdio>
#include "Interface.cpp"
#include "Parser.cpp"

int main() {
    InputBuffer inputBuffer;
    std::shared_ptr<Table> table = std::make_shared<Table>("Data.db");
    Parser parser;

    while(true){
        printPrompt();
        inputBuffer.readInput();

        if(inputBuffer.isMetaCommand()){
            switch(inputBuffer.performMetaCommand()){
                case MetaCommandResult::exit:
                    table->close();
                    printf("Exited Successfully\n");
                    exit(EXIT_SUCCESS);

                case MetaCommandResult::empty:
                    continue;

                case MetaCommandResult::unrecognized:
                    printf("Unrecognized command '%s'.\n", inputBuffer.str());
                    continue;
            }
        }

        switch(parser.parse(inputBuffer)){
            case PrepareResult::success:
                break;
            case PrepareResult::syntaxError:
                printf("Syntax Error. Could not parse statement\n");
                continue;
            case PrepareResult::stringTooLong :
                printf("String is too long.\n");
                continue;
            case PrepareResult::negativeID:
                printf("ID must be positive.\n");
                continue;
            case PrepareResult::invalidType:
                printf("Invalid Data Type.\n");
                continue;
            case PrepareResult::noSizeForString:
                printf("No size provided for string.\n");
                continue;
            case PrepareResult::unrecognized:
                printf("Unrecognized keyword at start of '%s'.\n", inputBuffer.str());
                continue;
            case PrepareResult::invalidOperator:
                printf("Invalid Operator\n");
                continue;
            case PrepareResult::comparisonOnDifferentRows:
                printf("Comparison on different rows is not allowed\n");
                continue;
            case PrepareResult::cannotCreateEmptyTable:
                printf("Cannot Create Empty Table. Please add some columns\n");
                continue;
            case PrepareResult::noTableName:
                printf("Please provide Table Name\n");
                continue;
            case PrepareResult::noInsertData:
                printf("No Data Provided to Insert\n");
                continue;
            case PrepareResult::noUpdateData:
                printf("No Data Provided to Update\n");
                continue;
            case PrepareResult::noCondition:
                printf("Provide Condition To Delete Selected Table using `where` clause.\n"
                       "To delete all entries use `delete table` instead\n");
                continue;
        }

        switch(Executor::execute(parser)){
            case ExecuteResult::success:
                printf("Executed.\n");
                break;
            case ExecuteResult::tableFull:
                printf("Error: Table full.\n");
                break;
            case ExecuteResult::faliure:
                printf("Action Failed\n");
                break;
        }
    }
    return 0;
}