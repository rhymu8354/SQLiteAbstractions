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

#include <ClusterMemberStore/Database.hpp>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <SystemAbstractions/IFileSystemEntry.hpp>
#include <vector>

namespace ClusterMemberStore {

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
        // These are meant to be used primarily (hopefully exclusively) by
        // database migrations.
        virtual void CreateTable(
            const std::string& tableName,
            const TableDefinition& tableDefinition
        ) override;
        virtual TableDefinitions DescribeTables() override;
        virtual void RenameTable(
            const std::string& oldTableName,
            const std::string& newTableName
        ) override;
        virtual void AddColumn(
            const std::string& tableName,
            const ColumnDefinition& columnDefinition
        ) override;
        virtual void DestroyColumn(
            const std::string& tableName,
            const std::string& columnName
        ) override;
        virtual void DestroyTable(
            const std::string& tableName
        ) override;

        // These are for general use by applications as well as database
        // migrations.
        virtual void CreateRow(
            const std::string& tableName,
            const ColumnDescriptors& columns
        ) override;
        virtual DataSet RetrieveRows(
            const std::string& tableName,
            const RowSelector& rowSelector,
            const ColumnSelector& columnSelector
        ) override;
        virtual size_t UpdateRows(
            const std::string& tableName,
            const RowSelector& rowSelector,
            const ColumnDescriptors& columns
        ) override;
        virtual size_t DestroyRows(
            const std::string& tableName,
            const RowSelector& rowSelector
        ) override;

        // These are especially designed for use by the Raft Consensus
        // Algorithm when installing snapshots of state from one server to
        // another.
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
