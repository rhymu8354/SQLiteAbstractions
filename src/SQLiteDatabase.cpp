/**
 * @file SQLiteDatabase.cpp
 *
 * This file contains the implementation of the
 * SQLiteClusterMemberStore::SQLiteDatabase class.
 */

#include <functional>
#include <sqlite3.h>
#include <SQLiteClusterMemberStore/SQLiteDatabase.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <StringExtensions/StringExtensions.hpp>
#include <unordered_set>

namespace {

    using namespace ClusterMemberStore;

    using DatabaseConnection = std::unique_ptr< sqlite3, std::function< void(sqlite3*) > >;
    using PreparedStatement = std::unique_ptr< sqlite3_stmt, std::function< void(sqlite3_stmt*) > >;

    std::string GetLastDatabaseError(const DatabaseConnection& db) {
        return sqlite3_errmsg(db.get());
    }

    void BindStatementParameter(
        const PreparedStatement& stmt,
        int index,
        const std::string& value
    ) {
        (void)sqlite3_bind_text(
            stmt.get(),
            index,
            value.data(),
            (int)value.length(),
            SQLITE_TRANSIENT
        );
    }

    PreparedStatement BuildStatement(
        const DatabaseConnection& db,
        const std::string& statement
    ) {
        sqlite3_stmt* statementRaw;
        if (
            sqlite3_prepare_v2(
                db.get(),
                statement.c_str(),
                (int)(statement.length() + 1), // sqlite wants count to include the null
                &statementRaw,
                NULL
            )
            != SQLITE_OK
        ) {
            const auto errmsg = GetLastDatabaseError(db);
            return nullptr;
        }
        return PreparedStatement(
            statementRaw,
            [](sqlite3_stmt* statementRaw){
                (void)sqlite3_finalize(statementRaw);
            }
        );
    }

    /**
     * Execute the given SQL statement on the given database.
     *
     * @param[in] db
     *     This is the handle to the database.
     *
     * @param[in] statement
     *     This is the SQL statement to execute.
     *
     * @return
     *     If the statement resulted in an error, a string describing
     *     the error is returned.  Otherwise, an empty string is returned.
     */
    std::string ExecuteStatement(
        const DatabaseConnection& db,
        const std::string& statement
    ) {
        char* errmsg = NULL;
        const auto result = sqlite3_exec(
            db.get(),
            statement.c_str(),
            NULL, NULL, &errmsg
        );
        if (result != SQLITE_OK) {
            std::string errmsgManaged(errmsg);
            sqlite3_free(errmsg);
            return errmsgManaged;
        }
        return "";
    }

    /**
     * Execute the given prepared SQL statement.
     *
     * @param[in] db
     *     This is the database for which execute the statement.
     *
     * @param[in] statement
     *     This is the prepared SQL statement to execute.
     *
     * @return
     *     If the statement resulted in an error, a string describing
     *     the error is returned.  Otherwise, an empty string is returned.
     */
    std::string ExecuteStatement(
        const DatabaseConnection& db,
        const PreparedStatement& statement
    ) {
        for(;;) {
            switch (sqlite3_step(statement.get())) {
                case SQLITE_DONE: {
                    return "";
                } break;

                case SQLITE_ROW: {
                } break;

                default: {
                    const auto errmsg = GetLastDatabaseError(db);
                    return errmsg;
                } break;
            }
        }
    }

    bool GetColumnBoolean(
        const PreparedStatement& stmt,
        int columnIndex
    ) {
        return (
            sqlite3_column_int(
                stmt.get(),
                columnIndex
            )
            != 0
        );
    }

    std::string GetColumnText(
        const PreparedStatement& stmt,
        int columnIndex
    ) {
        return (const char*)sqlite3_column_text(
            stmt.get(),
            columnIndex
        );
    }

    void ResetStatement(const PreparedStatement& stmt) {
        (void)sqlite3_reset(stmt.get());
    }

    struct StepStatementResults {
        bool done = false;
        bool error = false;
    };

    StepStatementResults StepStatement(const PreparedStatement& stmt) {
        StepStatementResults results;
        switch (sqlite3_step(stmt.get())) {
            case SQLITE_DONE: {
                results.done = true;
            } break;

            case SQLITE_ROW: {
            } break;

            default: {
                results.error = true;
            } break;
        }
        return results;
    }

}

namespace ClusterMemberStore {

    // Here we implement what we specified we would have in our interface.
    // This contains our private properties.
    struct SQLiteDatabase::Impl {
        // Properties

        DatabaseConnection db;
        PreparedStatement getTableNames;
        TableDefinitions tables;

        // Methods

        void ReadMetadata() {
            ResetStatement(getTableNames);
            tables.clear();
            StepStatementResults getTableNameResult;
            while (
                getTableNameResult = StepStatement(getTableNames),
                (
                    !getTableNameResult.done
                    && !getTableNameResult.error
                )
            ) {
                // In general this is a bad habit, to use string interpolation
                // to fill in parameters.  We really should try to use
                // bindings whenever possible.
                //
                // However, for SQLite PRAGMA statements, bindings don't work.
                // For reference:
                //     * https://stackoverflow.com/questions/39985599/parameter-binding-not-working-for-sqlite-pragma-table-info
                //     * http://sqlite.1065341.n5.nabble.com/PRAGMA-doesn-t-support-parameter-binds-td45863.html#a45866
                const auto tableName = GetColumnText(getTableNames, 0);
                TableDefinition table;
                auto getTableSchema = BuildStatement(
                    db,
                    StringExtensions::sprintf(
                        "PRAGMA table_info(%s)",
                        tableName.c_str()
                    )
                );
                StepStatementResults getTableSchemaResult;
                while (
                    getTableSchemaResult = StepStatement(getTableSchema),
                    (
                        !getTableSchemaResult.done
                        && !getTableSchemaResult.error
                    )
                ) {
                    table.columnDefinitions.push_back({
                        GetColumnText(getTableSchema, 1),
                        GetColumnText(getTableSchema, 2),
                        GetColumnBoolean(getTableSchema, 5),
                    });
                }
                tables[tableName] = std::move(table);
            }
        }
    };

    SQLiteDatabase::~SQLiteDatabase() noexcept = default;
    SQLiteDatabase::SQLiteDatabase(SQLiteDatabase&&) noexcept = default;
    SQLiteDatabase& SQLiteDatabase::operator=(SQLiteDatabase&&) noexcept = default;

    SQLiteDatabase::SQLiteDatabase()
        : impl_(new Impl())
    {
    }

    bool SQLiteDatabase::Open(const std::string& filePath) {
        sqlite3* dbRaw;
        if (sqlite3_open(filePath.c_str(), &dbRaw) != SQLITE_OK) {
            (void)sqlite3_close(dbRaw);
            return false;
        }
        impl_->db = DatabaseConnection(
            dbRaw,
            [](sqlite3* dbRaw){
                (void)sqlite3_close(dbRaw);
            }
        );
        impl_->getTableNames = BuildStatement(
            impl_->db,
            "SELECT name FROM SQLITE_MASTER WHERE TYPE='table'"
        );
        impl_->ReadMetadata();
        return true;
    }

    void SQLiteDatabase::CreateTable(
        const std::string& tableName,
        const TableDefinition& tableDefinition
    ) {
        std::ostringstream statement;
        statement << "CREATE TABLE " << tableName << " (";
        bool firstColumn = true;
        for (const auto& columnDefinition: tableDefinition.columnDefinitions) {
            if (!firstColumn) {
                statement << ", ";
            }
            firstColumn = false;
            statement << columnDefinition.name << ' ' << columnDefinition.type;
            if (columnDefinition.isKey) {
                statement << " PRIMARY KEY";
            }
        }
        statement << ')';
        (void)ExecuteStatement(
            impl_->db,
            statement.str()
        );
    }

    TableDefinitions SQLiteDatabase::DescribeTables() {
        return impl_->tables;
    }

    void SQLiteDatabase::RenameTable(
        const std::string& oldTableName,
        const std::string& newTableName
    ) {
        (void)ExecuteStatement(
            impl_->db,
            StringExtensions::sprintf(
                "ALTER TABLE %s RENAME TO %s",
                oldTableName.c_str(),
                newTableName.c_str()
            )
        );
    }

    void SQLiteDatabase::AddColumn(
        const std::string& tableName,
        const ColumnDefinition& columnDefinition
    ) {
        (void)ExecuteStatement(
            impl_->db,
            StringExtensions::sprintf(
                "ALTER TABLE %s ADD COLUMN %s %s%s",
                tableName.c_str(),
                columnDefinition.name.c_str(),
                columnDefinition.type.c_str(),
                (
                    columnDefinition.isKey
                    ? " PRIMARY KEY"
                    : ""
                )
            )
        );
    }

    void SQLiteDatabase::DestroyColumn(
        const std::string& tableName,
        const std::string& columnName
    ) {
        // Look up the metadata we have on this table.
        const auto tablesEntry = impl_->tables.find(tableName);
        if (tablesEntry == impl_->tables.end()) {
            return; // no such table
        }
        const auto& tableDefinition = tablesEntry->second;

        // Construct lists of columns for use in moving data
        // as well as recreating the columns.
        std::ostringstream keepColumns;
        std::ostringstream recreateColumns;
        bool firstColumn = true;
        bool foundColumnToBeDestroyed = false;
        for (const auto& columnDefinition: tableDefinition.columnDefinitions) {
            if (columnDefinition.name == columnName) {
                foundColumnToBeDestroyed = true;
                continue;
            }
            if (!firstColumn) {
                keepColumns << ',';
                recreateColumns << ", ";
            }
            firstColumn = false;
            keepColumns << columnDefinition.name;
            recreateColumns << columnDefinition.name << ' ' << columnDefinition.type;
            if (columnDefinition.isKey) {
                recreateColumns << " PRIMARY KEY";
            }
        }

        // Short-cut the process if no column was found with the given name.
        if (!foundColumnToBeDestroyed) {
            return;
        }

        // Perform the exotic-sequence™ which will drop the column by
        // completely copying the data we want to keep, reconstructing
        // the table without the column we want to drop, and finally
        // copying the data back.
        std::vector< std::string > statements;
        statements.push_back("BEGIN TRANSACTION");
        statements.push_back(
            StringExtensions::sprintf(
                "CREATE TEMPORARY TABLE %s_(%s)",
                tableName.c_str(),
                keepColumns.str().c_str()
            )
        );
        statements.push_back(
            StringExtensions::sprintf(
                "INSERT INTO %s_ SELECT %s FROM %s",
                tableName.c_str(),
                keepColumns.str().c_str(),
                tableName.c_str()
            )
        );
        statements.push_back(
            StringExtensions::sprintf(
                "DROP TABLE %s",
                tableName.c_str()
            )
        );
        statements.push_back(
            StringExtensions::sprintf(
                "CREATE TABLE %s (%s)",
                tableName.c_str(),
                recreateColumns.str().c_str()
            )
        );
        statements.push_back(
            StringExtensions::sprintf(
                "INSERT INTO %s SELECT %s FROM %s_",
                tableName.c_str(),
                keepColumns.str().c_str(),
                tableName.c_str()
            )
        );
        statements.push_back(
            StringExtensions::sprintf(
                "DROP TABLE %s_",
                tableName.c_str()
            )
        );
        statements.push_back("COMMIT");
        for (const auto& statement: statements) {
            (void)ExecuteStatement(impl_->db, statement);
        }
    }

    void SQLiteDatabase::DestroyTable(
        const std::string& tableName
    ) {
    }

    void SQLiteDatabase::CreateRow(
        const std::string& tableName,
        const ColumnDescriptors& columns
    ) {
    }

    DataSet SQLiteDatabase::RetrieveRows(
        const std::string& tableName,
        const RowSelector& rowSelector,
        const ColumnSelector& columnSelector
    ) {
        return {};
    }

    size_t SQLiteDatabase::UpdateRows(
        const std::string& tableName,
        const RowSelector& rowSelector,
        const ColumnDescriptors& columns
    ) {
        return 0;
    }

    size_t SQLiteDatabase::DestroyRows(
        const std::string& tableName,
        const RowSelector& rowSelector
    ) {
        return 0;
    }

    Blob SQLiteDatabase::CreateSnapshot() {
        return {};
    }

    void SQLiteDatabase::InstallSnapshot(const Blob& blob) {
    }

}
