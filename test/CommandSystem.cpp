#include "RadiantTest.h"

namespace test
{

using CommandSystemTest = RadiantTest;

TEST_F(CommandSystemTest, GetCommandSystem)
{
    const auto& mod = GlobalCommandSystem();
    EXPECT_EQ(mod.getName(), "CommandSystem");
}

TEST_F(CommandSystemTest, AddAndRunCommand)
{
    const char* COMMAND_NAME = "testRunCount";
    int runCount = 0;

    // Add a command which just logs the number of times it is called
    ASSERT_FALSE(GlobalCommandSystem().commandExists(COMMAND_NAME));
    GlobalCommandSystem().addCommand(COMMAND_NAME, [&](const cmd::ArgumentList&) { ++runCount; });
    EXPECT_TRUE(GlobalCommandSystem().commandExists(COMMAND_NAME));

    // Ensure that the call happens when we run the command
    GlobalCommandSystem().executeCommand(COMMAND_NAME);
    EXPECT_EQ(runCount, 1);
    GlobalCommandSystem().executeCommand(COMMAND_NAME);
    EXPECT_EQ(runCount, 2);
}

TEST_F(CommandSystemTest, AddAndRunCommandWithArgs)
{
    const char* COMMAND_NAME = "testCmdWithArgs";
    ASSERT_FALSE(GlobalCommandSystem().commandExists(COMMAND_NAME));

    // Create a test command which stores its args
    int runCount = 0;
    cmd::ArgumentList args;
    GlobalCommandSystem().addCommand(COMMAND_NAME,
                                     [&](const cmd::ArgumentList& a) {
                                         ++runCount;
                                         args = a;
                                     },
                                     {cmd::ARGTYPE_INT, cmd::ARGTYPE_STRING});

    // Call the command and check the args
    GlobalCommandSystem().executeCommand(COMMAND_NAME, 27, std::string("balls"));
    EXPECT_EQ(runCount, 1);
    ASSERT_EQ(args.size(), 2);
    EXPECT_EQ(args.at(0).getInt(), 27);
    EXPECT_EQ(args.at(1).getString(), "balls");

    // Calling the command with incorrect args does nothing (the command is not
    // called, but there is not currently a way to signal this to the caller
    // except via the message bus)
    GlobalCommandSystem().executeCommand(COMMAND_NAME, std::string("wrong"));
    EXPECT_EQ(runCount, 1);
}

TEST_F(CommandSystemTest, RunCommandSequence)
{
    const char* FIRST_COMMAND = "firstRunCountCommand";
    int firstRunCount = 0;
    const char* SECOND_COMMAND = "secondRunCountCommand";
    int secondRunCount = 0;

    // Register a command for each run count
    ASSERT_FALSE(GlobalCommandSystem().commandExists(FIRST_COMMAND));
    ASSERT_FALSE(GlobalCommandSystem().commandExists(SECOND_COMMAND));
    GlobalCommandSystem().addCommand(FIRST_COMMAND,
                                     [&](const cmd::ArgumentList&) { ++firstRunCount; });
    GlobalCommandSystem().addCommand(SECOND_COMMAND,
                                     [&](const cmd::ArgumentList&) { ++secondRunCount; });

    // Run a semicolon-separated sequence of both commands
    GlobalCommandSystem().execute("firstRunCountCommand; secondRunCountCommand");
    EXPECT_EQ(firstRunCount, 1);
    EXPECT_EQ(secondRunCount, 1);
    GlobalCommandSystem().execute("  secondRunCountCommand  ; firstRunCountCommand  ");
    EXPECT_EQ(firstRunCount, 2);
    EXPECT_EQ(secondRunCount, 2);
    GlobalCommandSystem().execute("secondRunCountCommand ;secondRunCountCommand");
    EXPECT_EQ(secondRunCount, 4);
}

TEST_F(CommandSystemTest, AddCheckedCommand)
{
    const char* COMMAND_NAME = "testCheckedCommand";
    bool commandEnabled = false;

    // Add a command which is conditionally enabled based on our variable flag
    ASSERT_FALSE(GlobalCommandSystem().commandExists(COMMAND_NAME));
    GlobalCommandSystem().addWithCheck(
        COMMAND_NAME, [](const cmd::ArgumentList&) {}, [&]() { return commandEnabled; }
    );

    // The flag should control the executability of the command
    EXPECT_FALSE(GlobalCommandSystem().canExecute(COMMAND_NAME));
    commandEnabled = true;
    EXPECT_TRUE(GlobalCommandSystem().canExecute(COMMAND_NAME));
    commandEnabled = false;
    EXPECT_FALSE(GlobalCommandSystem().canExecute(COMMAND_NAME));
}

}