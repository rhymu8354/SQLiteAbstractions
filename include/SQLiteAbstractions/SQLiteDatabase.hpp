#pragma once

/**
 * @file SQLiteDatabase.hpp
 *
 * This file specifies the SQLite implementation of an abstract interface for
 * C++ components to access a database in an exclusive manner in the context of
 * a member of a larger cluster (such as in the Raft Consensus Algorithm).  In
 * this context, the database represents one member's data store, exclusively
 * accessed by that member, and potentially overwritten completely if the
 * cluster leader installs a new snapshot.
 */

#include <DatabaseAbstractions/Database.hpp>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <SystemAbstractions/IFileSystemEntry.hpp>
#include <vector>

namespace DatabaseAbstractions {

    /**
     * This is the SQLite implementation of an abstract interface for
     * general-purpose access to some kind of relational database.
     */
    class SQLiteDatabase : public Database {
        // Lifecycle
    public:
        ~SQLiteDatabase() noexcept;
        SQLiteDatabase(const SQLiteDatabase&) = delete;
        SQLiteDatabase(SQLiteDatabase&&) noexcept;
        SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;
        SQLiteDatabase& operator=(SQLiteDatabase&&) noexcept;

        // Methods
    public:
        SQLiteDatabase();
        bool Open(const std::string& filePath);

        // Database
    public:
        virtual BuildStatementResults BuildStatement(
            const std::string& statement
        ) override;
        virtual std::string ExecuteStatement(const std::string& statement) override;
        virtual Blob CreateSnapshot() override;
        virtual void InstallSnapshot(const Blob& blob) override;

        // Properties
    private:
        /**
         * This is the type of structure that contains the private
         * properties of the instance.  It is defined in the implementation
         * and declared here to ensure that it is scoped inside the class.
         */
        struct Impl;

        /**
         * This contains the private properties of the instance.
         */
        std::unique_ptr< Impl > impl_;
    };

}
