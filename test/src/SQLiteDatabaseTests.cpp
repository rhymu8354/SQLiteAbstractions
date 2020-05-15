/**
 * @file SQLiteDatabaseTests.cpp
 *
 * This module contains the unit tests of the
 * SQLiteAbstractions class.
 */

#include <gtest/gtest.h>
#include <SQLiteAbstractions/SQLiteDatabase.hpp>
#include <memory>
#include <set>
#include <sqlite3.h>
#include <SystemAbstractions/File.hpp>
#include <unordered_set>
#include <vector>

using namespace DatabaseAbstractions;

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
        "CREATE TABLE kv (key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE npcs (entity INT PRIMARY KEY, name TEXT, job TEXT, time REAL)",
        "CREATE TABLE quests (npc INT, quest INT, completed BOOLEAN)",
        "INSERT INTO kv VALUES ('foo', 'bar')",
        "INSERT INTO kv VALUES ('spam', NULL)",
        "INSERT INTO npcs VALUES (1, 'Alex', 'Armorer', 4.321)",
        "INSERT INTO npcs VALUES (2, 'Bob', 'Banker', NULL)",
        "INSERT INTO quests VALUES (1, 42, 0)",
        "INSERT INTO quests VALUES (1, 43, NULL)",
        "INSERT INTO quests VALUES (2, 43, 1)",
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

TEST_F(SQLiteDatabaseTests, PreparedStatement_Step_No_Data) {
    // Arrange
    DatabaseConnection comparisonDb;
    const std::string statementText = "INSERT INTO kv (key, value) VALUES ('hello', 'world')";
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            statementText,
        }
    );
    auto statement = db.BuildStatement(statementText).statement;

    // Act
    const auto stepResult = statement->Step();

    // Assert
    EXPECT_TRUE(stepResult.done);
    EXPECT_TRUE(stepResult.error.empty());
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_Step_One_Row) {
    // Arrange
    auto statement = db.BuildStatement(
        "SELECT quest FROM quests WHERE npc = 2"
    ).statement;

    // Act
    const auto step1 = statement->Step();
    const auto quest1 = statement->FetchColumn(0, Value::Type::Integer);
    const auto step2 = statement->Step();

    // Assert
    EXPECT_FALSE(step1.done);
    EXPECT_TRUE(step1.error.empty());
    EXPECT_EQ(43, (int)quest1);
    EXPECT_TRUE(step2.done);
    EXPECT_TRUE(step2.error.empty());
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_Step_Multiple_Rows) {
    // Arrange
    auto statement = db.BuildStatement(
        "SELECT quest FROM quests WHERE npc = 1"
    ).statement;

    // Act
    const auto step1 = statement->Step();
    const auto quest1 = statement->FetchColumn(0, Value::Type::Integer);
    const auto step2 = statement->Step();
    const auto quest2 = statement->FetchColumn(0, Value::Type::Integer);
    const auto step3 = statement->Step();

    // Assert
    EXPECT_FALSE(step1.done);
    EXPECT_TRUE(step1.error.empty());
    EXPECT_EQ(42, (int)quest1);
    EXPECT_FALSE(step2.done);
    EXPECT_TRUE(step2.error.empty());
    EXPECT_EQ(43, (int)quest2);
    EXPECT_TRUE(step3.done);
    EXPECT_TRUE(step3.error.empty());
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_Step_Error) {
    // Arrange
    auto statement = db.BuildStatement(
        "INSERT INTO npcs (entity) VALUES (1)"
    ).statement;

    // Act
    const auto stepResults = statement->Step();

    // Assert
    EXPECT_TRUE(stepResults.done);
    EXPECT_FALSE(stepResults.error.empty());
    VerifyNoChanges();
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_BindParameter_Text) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            "INSERT INTO kv (key, value) VALUES ('hello', 'world')",
        }
    );
    auto statement = db.BuildStatement(
        "INSERT INTO kv (key, value) VALUES ('hello', ?)"
    ).statement;

    // Act
    statement->BindParameter(0, "world");

    // Assert
    (void)statement->Step();
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_BindParameter_Integer) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            "INSERT INTO quests (npc, quest) VALUES (1, 99)",
        }
    );
    auto statement = db.BuildStatement(
        "INSERT INTO quests (npc, quest) VALUES (1, ?)"
    ).statement;

    // Act
    statement->BindParameter(0, 99);

    // Assert
    (void)statement->Step();
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_BindParameter_Real) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            "UPDATE npcs SET time = 1.23 WHERE entity = 1",
        }
    );
    auto statement = db.BuildStatement(
        "UPDATE npcs SET time = ? WHERE entity = 1"
    ).statement;

    // Act
    statement->BindParameter(0, 1.23);

    // Assert
    (void)statement->Step();
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_BindParameter_Boolean) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            "UPDATE quests SET completed = 1 WHERE npc = 1",
        }
    );
    auto statement = db.BuildStatement(
        "UPDATE quests SET completed = ? WHERE npc = 1"
    ).statement;

    // Act
    statement->BindParameter(0, true);

    // Assert
    (void)statement->Step();
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_BindParameter_Null) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            "UPDATE npcs SET job = NULL WHERE entity = 1",
        }
    );
    auto statement = db.BuildStatement(
        "UPDATE npcs SET job = ? WHERE entity = 1"
    ).statement;
    statement->BindParameter(0, 42);

    // Act
    statement->BindParameter(0, nullptr);

    // Assert
    (void)statement->Step();
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_BindParameters) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            "UPDATE npcs SET job = 'guard', time = 1.23 WHERE entity = 1",
        }
    );
    auto statement = db.BuildStatement(
        "UPDATE npcs SET job = ?, time = ? WHERE entity = ?"
    ).statement;

    // Act
    statement->BindParameters({"guard", 1.23, 1});

    // Assert
    (void)statement->Step();
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_Reset) {
    // Arrange
    DatabaseConnection comparisonDb;
    ReconstructDatabase(
        comparisonDbFilePath,
        defaultDbInitStatements,
        comparisonDb,
        {
            "INSERT INTO quests (npc, quest) VALUES (1, 99)",
            "INSERT INTO quests (npc, quest) VALUES (2, 76)",
        }
    );
    auto statement = db.BuildStatement(
        "INSERT INTO quests (npc, quest) VALUES (?, ?)"
    ).statement;
    statement->BindParameters({1, 99});
    (void)statement->Step();

    // Act
    statement->Reset();

    // Assert
    statement->BindParameters({2, 76});
    (void)statement->Step();
    VerifySerialization(comparisonDb);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_FetchColumn_Null) {
    // Arrange
    auto statement = db.BuildStatement(
        "SELECT value FROM kv WHERE key = 'spam'"
    ).statement;
    (void)statement->Step();

    // Act
    const auto value = statement->FetchColumn(0, Value::Type::Text);

    // Assert
    EXPECT_EQ(Value::Type::Null, value.GetType());
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_FetchColumn_Text) {
    // Arrange
    auto statement = db.BuildStatement(
        "SELECT value FROM kv WHERE key = 'foo'"
    ).statement;
    (void)statement->Step();

    // Act
    const auto value = statement->FetchColumn(0, Value::Type::Text);

    // Assert
    EXPECT_EQ(Value::Type::Text, value.GetType());
    EXPECT_EQ("bar", (const std::string&)value);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_FetchColumn_Integer) {
    // Arrange
    auto statement = db.BuildStatement(
        "SELECT quest FROM quests WHERE npc = 2"
    ).statement;
    (void)statement->Step();

    // Act
    const auto value = statement->FetchColumn(0, Value::Type::Integer);

    // Assert
    EXPECT_EQ(Value::Type::Integer, value.GetType());
    EXPECT_EQ(43, (int)value);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_FetchColumn_Real) {
    // Arrange
    auto statement = db.BuildStatement(
        "SELECT time FROM npcs WHERE entity = 1"
    ).statement;
    (void)statement->Step();

    // Act
    const auto value = statement->FetchColumn(0, Value::Type::Real);

    // Assert
    EXPECT_EQ(Value::Type::Real, value.GetType());
    EXPECT_EQ(4.321, (double)value);
}

TEST_F(SQLiteDatabaseTests, PreparedStatement_FetchColumn_Boolean) {
    // Arrange
    auto statement = db.BuildStatement(
        "SELECT completed FROM quests WHERE npc = 2"
    ).statement;
    (void)statement->Step();

    // Act
    const auto value = statement->FetchColumn(0, Value::Type::Boolean);

    // Assert
    EXPECT_EQ(Value::Type::Boolean, value.GetType());
    EXPECT_TRUE((bool)value);
}

TEST_F(SQLiteDatabaseTests, CreateSnapshot) {
    // TODO
    ASSERT_TRUE(false);
}

TEST_F(SQLiteDatabaseTests, InstallSnapshot) {
    // TODO
    ASSERT_TRUE(false);
}
