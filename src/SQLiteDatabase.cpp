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

        // Methods

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
        return true;
    }

    BuildStatementResults SQLiteDatabase::BuildStatement(
        const std::string& statement
    ) {
        BuildStatementResults results;
        sqlite3_stmt* statementRaw;
        if (
            sqlite3_prepare_v2(
                impl_->db.get(),
                statement.c_str(),
                (int)(statement.length() + 1), // sqlite wants count to include the null
                &statementRaw,
                NULL
            )
            != SQLITE_OK
        ) {
            results.error = GetLastDatabaseError(impl_->db);
        }
        (void)sqlite3_finalize(statementRaw);
        // return PreparedStatement(
        //     statementRaw,
        //     [](sqlite3_stmt* statementRaw){
        //         (void)sqlite3_finalize(statementRaw);
        //     }
        // );
        return results;
    }

    std::string SQLiteDatabase::ExecuteStatement(const std::string& statement) {
        char* errmsg;
        if (
            sqlite3_exec(
                impl_->db.get(),
                statement.c_str(),
                NULL,
                NULL,
                &errmsg
            ) == SQLITE_ERROR
        ) {
            std::string error(errmsg);
            sqlite3_free(errmsg);
            return error;
        } else {
            return "";
        }
    }

    Blob SQLiteDatabase::CreateSnapshot() {
        return {};
    }

    void SQLiteDatabase::InstallSnapshot(const Blob& blob) {
    }

}
