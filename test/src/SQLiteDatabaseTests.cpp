/**
 * @file SQLiteDatabaseTests.cpp
 *
 * This module contains the unit tests of the
 * SQLiteClusterMemberStore class.
 */

#include <gtest/gtest.h>
#include <SQLiteClusterMemberStore/SQLiteDatabase.hpp>
#include <memory>
#include <set>
#include <sqlite3.h>
#include <SystemAbstractions/File.hpp>
#include <unordered_set>
#include <vector>

using namespace ClusterMemberStore;

/**
 * This is the base for test fixtures used to test the SMTP library.
 */
struct SQLiteDatabaseTests
    : public ::testing::Test
{
    // Types

    using DatabaseConnection = std::unique_ptr< sqlite3, std::function< void(sqlite3*) > >;
    using SQLStatements = std::vector< std::string >;

    // Properties

    SQLiteDatabase db;
    const SQLStatements defaultDbInitStatements{
        "CREATE TABLE kv (key text PRIMARY KEY, value text)",
        "CREATE TABLE npcs (entity int PRIMARY KEY, name text, job text)",
        "CREATE TABLE quests (npc int, quest int)",
        "INSERT INTO kv VALUES ('foo', 'bar')",
        "INSERT INTO npcs VALUES (1, 'Alex', 'Armorer')",
        "INSERT INTO npcs VALUES (2, 'Bob', 'Banker')",
        "INSERT INTO quests VALUES (1, 42)",
        "INSERT INTO quests VALUES (1, 43)",
        "INSERT INTO quests VALUES (2, 43)",
    };
    std::string defaultDbFilePath;
    std::string comparisonDbFilePath;
    std::string startingSerialization;

    // Methods

    /**
     * Open the SQLite database at the given file path and return
     * a handle to it.
     *
     * @param[in] filePath
     *     This is the path to the database to open.
     *
     * @param[out] db
     *     This is where to store the handle to the database.
     *
     * @return
     *     A handle to the database is returned.
     *
     * @retval nullptr
     *     This is returned if the database could not be opened.
     */
    void OpenDatabase(
        const std::string& filePath,
        DatabaseConnection& db
    ) {
        sqlite3* dbRaw;
        if (sqlite3_open(filePath.c_str(), &dbRaw) != SQLITE_OK) {
            std::string errmsg(sqlite3_errmsg(dbRaw));
            (void)sqlite3_close(dbRaw);
            FAIL() << errmsg;
        }
        db = DatabaseConnection(
            dbRaw,
            [](sqlite3* dbRaw){
                (void)sqlite3_close(dbRaw);
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
     */
    void ExecuteStatement(
        const DatabaseConnection& db,
        const std::string& statement
    ) {
        char* errmsg = NULL;
        if (
            sqlite3_exec(
                db.get(),
                statement.c_str(),
                NULL, NULL, &errmsg
            )
            != SQLITE_OK
        ) {
            std::string errmsgManaged(errmsg);
            sqlite3_free(errmsg);
            FAIL() << errmsgManaged;
        }
    }

    /**
     * Blow away the previous database (if any) at the given path, and
     * construct a new database from the given SQL statements.
     *
     * @param[in] filePath
     *     This is the path to the database to reconstruct.
     *
     * @param[in] initStatements
     *     These are the SQL statements to execute in order to construct
     *     the test database.
     *
     * @param[out] reconstructedDb
     *     This is where to store the handle to the reconstructed database.
     *
     * @param[in] extraStatements
     *     This is an optional list of extra SQL statements to execute
     *     after the `initStatements`.
     */
    void ReconstructDatabase(
        const std::string& filePath,
        const SQLStatements& initStatements,
        DatabaseConnection& reconstructedDb,
        const SQLStatements& extraStatements = {}
    ) {
        SystemAbstractions::File dbFile(filePath);
        dbFile.Destroy();
        DatabaseConnection db;
        OpenDatabase(filePath, db);
        for (const auto& statement: initStatements) {
            ExecuteStatement(db, statement);
        }
        for (const auto& statement: extraStatements) {
            ExecuteStatement(db, statement);
        }
        reconstructedDb = std::move(db);
    }

    void SerializeDatabase(
        const DatabaseConnection& db,
        std::string& serialization
    ) {
        sqlite3_int64 serializationSize;
        const auto serializationRaw = sqlite3_serialize(
            db.get(),
            "main",
            &serializationSize,
            0
        );
        serialization.assign(
            serializationRaw,
            serializationRaw + serializationSize
        );
        sqlite3_free(serializationRaw);
    }

    void VerifySerialization(const std::string& expected) {
        std::string actual;
        DatabaseConnection db;
        OpenDatabase(defaultDbFilePath, db);
        SerializeDatabase(db, actual);
        EXPECT_TRUE(expected == actual);
    }

    void VerifySerialization(const DatabaseConnection& otherDb) {
        std::string expected;
        SerializeDatabase(otherDb, expected);
        VerifySerialization(expected);
    }

    void VerifyNoChanges() {
        VerifySerialization(startingSerialization);
    }

    // ::testing::Test

    virtual void SetUp() override {
        defaultDbFilePath = SystemAbstractions::File::GetExeParentDirectory() + "/test.db";
        comparisonDbFilePath = SystemAbstractions::File::GetExeParentDirectory() + "/test2.db";
        DatabaseConnection dbInit;
        ReconstructDatabase(
            defaultDbFilePath,
            defaultDbInitStatements,
            dbInit
        );
        SerializeDatabase(
            dbInit,
            startingSerialization
        );
        (void)db.Open(defaultDbFilePath);
    }

    virtual void TearDown() override {
    }

};

TEST_F(SQLiteDatabaseTests, Verify_SQLite_Serialization_Is_Bit_Exact_For_Same_Database_State) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb
    );

    // Act

    // Assert
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, BuildStatement) {
    // Arrange

    // Act
    const auto buildResults1 = db.BuildStatement("SELECT entity FROM npcs");
    const auto buildResults2 = db.BuildStatement("SELECT foo FROM bar");

    // Assert
    EXPECT_TRUE(buildResults1.error.empty());
    EXPECT_FALSE(buildResults2.error.empty());
}

TEST_F(SQLiteDatabaseTests, ExecuteStatement) {
    // Arrange

    // Act
    const auto error1 = db.ExecuteStatement("SELECT entity FROM npcs");
    const auto error2 = db.ExecuteStatement("SELECT foo FROM bar");

    // Assert
    EXPECT_TRUE(error1.empty());
    EXPECT_FALSE(error2.empty());
}

TEST_F(SQLiteDatabaseTests, CreateSnapshot) {
    // TODO
    ASSERT_TRUE(false);
}

TEST_F(SQLiteDatabaseTests, InstallSnapshot) {
    // TODO
    ASSERT_TRUE(false);
}
