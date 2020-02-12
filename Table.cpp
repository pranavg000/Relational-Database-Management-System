//
// Created by Yogesh Kumar on 2020-01-31.
//
#include "HeaderFiles/Table.h"

// =============================================
//                  TABLE
// =============================================

Table::Table(std::string tableName, const std::string& fileName){
    try{
        this->pager = std::make_unique<Pager<Page>>(fileName.c_str());
    }
    catch(...){
        throw;
    }
    // TODO: Read first page to get metadata
    // Metadata:-
    // 1. Column Names
    // 2. Column Types
    // 3. Column Size
    // 4. Empty Rows
    this->tableOpen = true;
    this->tableName = std::move(tableName);
    this->numRows = 0;
    this->rowSize = 0;
    this->rowsPerPage = 0;
    this->rowStackPtr = 0;
    this->rowStackOffset = 0;
}

Table::~Table(){
    this->close();
}

bool Table::close(){
    if(tableOpen) return pager->close();
    return false;
}

Cursor Table::start(){
    Cursor cursor(this);
    cursor.row = 0;
    cursor.endOfTable = (numRows == 0);
    return cursor;
}

Cursor Table::end(){
    Cursor cursor(this);
    cursor.row = numRows;
    cursor.endOfTable = true;
    return cursor;
}

void Table::createColumns(std::vector<std::string>&& columnNames_, std::vector<DataType>&& columnTypes_, std::vector<uint32_t>&& columnSizes_){
    this->columnNames = std::move(columnNames_);
    this->columnSizes = std::move(columnSizes_);
    this->columnTypes = std::move(columnTypes_);
    this->createColumnIndex();
}

void Table::createColumnIndex(){
    int32_t count = columnNames.size();
    for(int index = 0; index < count; ++index){
        columnIndex[columnNames[index]] = index;
    }
}

void Table::storeMetadata() {
    Page* page = pager->header.get();
    serailizeColumnMetadata(page->buffer.get());
    page->hasUncommitedChanges = true;
    pager->flush(0);
    calculateRowInfo();

    trees.reserve(columnNames.size());
    for(int i = 0; i < columnNames.size(); ++i) trees.emplace_back(nullptr);
}

void Table::loadMetadata() {
    Page* page = pager->header.get();
    deSerailizeColumnMetadata(page->buffer.get());
    this->createColumnIndex();
    calculateRowInfo();

    trees.reserve(columnNames.size());
    for(int i = 0; i < columnNames.size(); ++i) trees.emplace_back(nullptr);
}

void Table::calculateRowInfo(){
    this->rowSize = 0;
    for(int32_t size: columnSizes){
        this->rowSize += size;
    }
    this->rowsPerPage = PAGE_SIZE/rowSize;
    int32_t count = columnSizes.size();
    this->indexed.assign(count, false);
    this->stackPtr.assign(count, 0);
}

void Table::serailizeColumnMetadata(char* buffer){
    int32_t size1 = columnNames.size();
    int32_t size2 = columnTypes.size();
    int32_t size3 = columnSizes.size();

    if(!(size1 == size2 && size2 == size3)){
        printf("Failed To Serialize Metadata.\nInconsistent Metadata\n");
        return;
    }

    int32_t offset = 0;

    // Write Number of Rows
    this->numRows = 0;
    memcpy(buffer + offset, &numRows, sizeof(row_t));
    offset += sizeof(row_t);

    // Write Number of columns
    memcpy(buffer + offset, &size1, sizeof(int32_t));
    offset += sizeof(int32_t);

    for(const std::string& str: columnNames){
        strncpy(buffer + offset, str.c_str(), MAX_COLUMN_SIZE);
        offset += MAX_COLUMN_SIZE;
    }

    for(uint32_t size: columnSizes){
        memcpy(buffer + offset, &size, sizeof(int32_t));
        offset += sizeof(int32_t);
    }

    for(const DataType& type: columnTypes){
        memcpy(buffer + offset, &type, sizeof(DataType));
        offset += sizeof(DataType);
    }
    rowStackPtr = 0;
    memcpy(buffer + offset, &rowStackPtr, sizeof(int32_t));
    rowStackOffset = offset + sizeof(int32_t);
}

void Table::deSerailizeColumnMetadata(char* metadataBuffer) {
    char colName[MAX_COLUMN_SIZE + 1] = {0};
    int32_t colSize;
    DataType colType;
    int32_t offset = 0;
    int32_t size;

    memcpy(&this->numRows, metadataBuffer + offset, sizeof(row_t));
    offset += sizeof(row_t);
    memcpy(&size, metadataBuffer + offset, sizeof(int32_t));
    offset += sizeof(int32_t);

    columnNames.reserve(size);
    columnSizes.reserve(size);
    columnTypes.reserve(size);

    for(int32_t i = 0; i < size; ++i){
        strncpy(colName, (metadataBuffer + offset), MAX_COLUMN_SIZE);
        columnNames.emplace_back(colName);
        offset += MAX_COLUMN_SIZE;
    }

    for(int32_t i = 0; i < size; ++i){
        memcpy(&colSize, (metadataBuffer + offset), sizeof(int32_t));
        columnSizes.emplace_back(colSize);
        offset += sizeof(int32_t);
    }

    for(int32_t i = 0; i < size; ++i){
        memcpy(&colType, (metadataBuffer + offset), sizeof(DataType));
        columnTypes.emplace_back(colType);
        offset += sizeof(DataType);
    }

    memcpy(&rowStackPtr, (metadataBuffer + offset), sizeof(int32_t));
    rowStackOffset = offset + sizeof(int32_t);
}

row_t Table::nextFreeRowLocation(){
    if(rowStackPtr == 0) return numRows;
    Page* page = pager->header.get();
    char* buffer = page->buffer.get();
    int32_t offset = (rowStackPtr - 1) * sizeof(row_t) + rowStackOffset;
    row_t nextRow;
    memcpy(&nextRow, buffer + offset, sizeof(row_t));
    rowStackPtr--;
    memcpy(buffer, &rowStackPtr, sizeof(int32_t));
    return nextRow;
}

void Table::addFreeRowLocation(row_t location){
    Page* page = pager->header.get();
    char* buffer = page->buffer.get();
    int32_t offset = rowStackPtr * sizeof(row_t) + sizeof(int32_t);
    rowStackPtr++;
    if(offset + sizeof(row_t) > PAGE_SIZE){
        printf("Stack Overflow occurred in main table.\n");
        throw std::runtime_error("STACK OVERFLOWS HEADER PAGE");
    }
    memcpy(buffer + offset, &location, sizeof(row_t));
}

int32_t Table::getRowSize() const{
    return this->rowSize;
}

void Table::increaseRowCount() {
    this->numRows++;
    Page* page = pager->header.get();
    char* buffer = page->buffer.get();
    memcpy(buffer, &numRows, sizeof(row_t));
    page->hasUncommitedChanges = true;
}

bool Table::createIndex(int index, const std::string& filename){
    if(!indexed[index]) return true;
    int32_t branchingFactor;
    switch(columnTypes[index]){
        case DataType::Int:
            trees[index] = std::make_unique<BPTree<int>>(filename.c_str(), 2, columnSizes[index]);
            break;
        case DataType::Float:
            trees[index] = std::make_unique<BPTree<float>>(filename.c_str(), floatBranchingFactor, columnSizes[index]);
            break;
        case DataType::Char:
            trees[index] = std::make_unique<BPTree<char>>(filename.c_str(), charBranchingFactor, columnSizes[index]);
            break;
        case DataType::Bool:
            trees[index] = std::make_unique<BPTree<bool>>(filename.c_str(), boolBranchingFactor, columnSizes[index]);
            break;
        case DataType::String:
            branchingFactor = BRANCHING_FACTOR(columnSizes[index]);
            trees[index] = std::make_unique<BPTree<dbms::string>>(filename.c_str(), branchingFactor, columnSizes[index]);
            break;
    }
    anyIndex = index;
    tableIsIndexed = true;
    return true;
}

bool Table::insertBTree(std::vector<std::string>& data, row_t row){
    static pkey_t pkey = 1;
    for(int i = 0; i < indexed.size(); ++i){
        if(!indexed[i]) continue;
        bool res;
        switch(columnTypes[i]){
            case DataType::Int:
                res = dynamic_cast<BPTree<int>*>(trees[i].get())->insert(data[i], pkey, row);
                break;
            case DataType::Float:
                res = dynamic_cast<BPTree<float>*>(trees[i].get())->insert(data[i], pkey, row);
                break;
            case DataType::Char:
                res = dynamic_cast<BPTree<char>*>(trees[i].get())->insert(data[i], pkey, row);
                break;
            case DataType::Bool:
                res = dynamic_cast<BPTree<bool>*>(trees[i].get())->insert(data[i], pkey, row);
                break;
            case DataType::String:
                BPTNode<dbms::string>::pKeyOffset = P_KEY_OFFSET(columnSizes[i]);
                BPTNode<dbms::string>::childOffset = CHILD_OFFSET(columnSizes[i]);
                res = dynamic_cast<BPTree<dbms::string>*>(trees[i].get())->insert(data[i], pkey, row);
                break;
        }
        if(!res) return false;
    }
    ++pkey;
    return true;
}