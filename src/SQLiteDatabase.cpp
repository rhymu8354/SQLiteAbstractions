/**
 * @file SQLiteDatabase.cpp
 *
 * This file contains the implementation of the
 * SQLiteAbstractions::SQLiteDatabase class.
 */

#include <functional>
#include <sqlite3.h>
#include <SQLiteAbstractions/SQLiteDatabase.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <StringExtensions/StringExtensions.hpp>
#include <SystemAbstractions/File.hpp>
#include <unordered_set>

namespace {

    using namespace DatabaseAbstractions;

    using DatabaseConnection = std::shared_ptr< sqlite3 >;

    std::string GetLastDatabaseError(const DatabaseConnection& db) {
        return sqlite3_errmsg(db.get());
    }

    struct SQliteStatement
        : public PreparedStatement
    {
        // Properties

        sqlite3_stmt* statement = nullptr;
        DatabaseConnection db;

        // Lifecycle

        ~SQliteStatement() noexcept {
            Drop();
        }

        // Do not allow copying.
        SQliteStatement(const SQliteStatement&) = delete;
        SQliteStatement& operator=(const SQliteStatement&) = delete;

        SQliteStatement(SQliteStatement&& other) noexcept {
            *this = std::move(other);
        }

        SQliteStatement& operator=(SQliteStatement&& other) noexcept {
            Drop();
            statement = other.statement;
            other.statement = nullptr;
        }

        // Constructor

        explicit SQliteStatement(
            sqlite3_stmt* statement,
            DatabaseConnection& db
        )
            : statement(statement)
            , db(db)
        {
        }

        // Methods

        void Drop() {
            if (statement) {
                (void)sqlite3_finalize(statement);
            }
        }

        // PreparedStatement

        virtual void BindParameter(
            int index,
            const Value& value
        ) override {
            switch(value.GetType()) {
                case Value::Type::Text: {
                    const std::string& valueAsString = value;
                    (void)sqlite3_bind_text(
                        statement,
                        index + 1,
                        valueAsString.c_str(),
                        (int)valueAsString.length(),
                        SQLITE_TRANSIENT
                    );
                } break;

                case Value::Type::Integer: {
                    (void)sqlite3_bind_int64(
                        statement,
                        index + 1,
                        (sqlite3_int64)(intmax_t)value
                    );
                } break;

                case Value::Type::Real: {
                    (void)sqlite3_bind_double(
                        statement,
                        index + 1,
                        (double)value
                    );
                } break;

                case Value::Type::Boolean: {
                    (void)sqlite3_bind_int(
                        statement,
                        index + 1,
                        (bool)value ? 1 : 0
                    );
                } break;

                case Value::Type::Null: {
                    (void)sqlite3_bind_null(
                        statement,
                        index + 1
                    );
                } break;

                default: break;
            }
        }

        virtual void BindParameters(std::initializer_list< const Value > values) override {
            int index = 0;
            for (
                auto value = values.begin();
                value != values.end();
                ++value, ++index
            ) {
                BindParameter(index, *value);
            }
        }

        virtual Value FetchColumn(int index, Value::Type type) override {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                return Value(nullptr);
            }
            switch (type) {
                case Value::Type::Text: {
                    return Value(
                        std::string(
                            (const char*)sqlite3_column_text(statement, index),
                            (size_t)sqlite3_column_bytes(statement, index)
                        )
                    );
                }

                case Value::Type::Integer: {
                    return Value(sqlite3_column_int64(statement, index));
                }

                case Value::Type::Real: {
                    return Value(sqlite3_column_double(statement, index));
                }

                case Value::Type::Boolean: {
                    return Value(sqlite3_column_int(statement, index) != 0);
                }

                default: return Value();
            }
        }

        virtual void Reset() override {
            (void)sqlite3_reset(statement);
        }

        virtual StepStatementResults Step() override {
            StepStatementResults results;
            switch (sqlite3_step(statement)) {
                case SQLITE_DONE: {
                    results.done = true;
                } break;

                case SQLITE_ROW: {
                    results.done = false;
                } break;

                default: {
                    results.done = true;
                    results.error = GetLastDatabaseError(db);
                } break;
            }
            return results;
        }
    };

}

namespace DatabaseAbstractions {

    // Here we implement what we specified we would have in our interface.
    // This contains our private properties.
    struct SQLiteDatabase::Impl {
        // Properties

        std::string filePath;
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
        impl_->filePath = filePath;
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
            == SQLITE_OK
        ) {
            auto managedStatement = std::make_shared< SQliteStatement >(
                statementRaw,
                impl_->db
            );
            results.statement = std::move(managedStatement);
        } else {
            results.error = GetLastDatabaseError(impl_->db);
        }
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
        sqlite3_int64 size;
        const auto serialization = sqlite3_serialize(impl_->db.get(), "main", &size, 0);
        return Blob(
            serialization,
            serialization + size
        );
    }

    std::string SQLiteDatabase::InstallSnapshot(const Blob& blob) {
        impl_->db = nullptr;
        SystemAbstractions::File dbFile(impl_->filePath);
        if (!dbFile.OpenReadWrite()) {
            return "Unable to open the database file for writing";
        }
        if (dbFile.Write(blob) != blob.size()) {
            return "Unable to write to database file";
        }
        if (!dbFile.SetSize(blob.size())) {
            return "Unable to set the end of the database file";
        }
        dbFile.Close();
        if (!Open(impl_->filePath)) {
            return "Unable to open database after installing snapshot";
        }
        return "";
    }

}
