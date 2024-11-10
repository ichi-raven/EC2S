#include <gtest/gtest.h>
#include "../include/Application.hpp"

// Common region class for testing
class TestCommonRegion
{
public:
    int value = 0;
};

// Enum representing test states
enum class TestState
{
    Initial,
    State1,
    State2,
    Final
};

// Test state classes
class InitialState : public ec2s::State<TestState, TestCommonRegion>
{
    GEN_STATE_CTOR_ONLY(InitialState, TestState, TestCommonRegion);

public:
    virtual ~InitialState()
    {
    }

    void init() override
    {
        getCommonRegion()->value = 1;
    }
    void update() override
    {
        changeState(TestState::State1);
    }
};

class State1 : public ec2s::State<TestState, TestCommonRegion>
{
    GEN_STATE_CTOR_ONLY(State1, TestState, TestCommonRegion);

public:
    virtual ~State1() override
    {
    }

    void init() override
    {
        getCommonRegion()->value = 2;
    }
    void update() override
    {
        changeState(TestState::State2);
    }
};

class State2 : public ec2s::State<TestState, TestCommonRegion>
{
    GEN_STATE_CTOR_ONLY(State2, TestState, TestCommonRegion);

public:
    virtual ~State2() override
    {
    }

    void init() override
    {
        getCommonRegion()->value = 3;
    }
    void update() override
    {
        changeState(TestState::Final);
    }
};

class FinalState : public ec2s::State<TestState, TestCommonRegion>
{
    GEN_STATE_CTOR_ONLY(FinalState, TestState, TestCommonRegion);

public:
    virtual ~FinalState() override
    {
    }

    void init() override
    {
        getCommonRegion()->value = 4;
    }
    void update() override
    {
        exitApplication();
    }
};

// Test fixture for state machine tests
class StateMachineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        app.addState<InitialState>(TestState::Initial);
        app.addState<State1>(TestState::State1);
        app.addState<State2>(TestState::State2);
        app.addState<FinalState>(TestState::Final);
    }

    ec2s::Application<TestState, TestCommonRegion> app;
};

// Test basic state transitions
TEST_F(StateMachineTest, BasicStateTransition)
{
    app.init(TestState::Initial);
    EXPECT_EQ(app.mpCommonRegion->value, 1);  // Check Initial state initialization

    app.update(); 
    EXPECT_EQ(app.mpCommonRegion->value, 1);
    // Initial -> State1
    app.update();  
    EXPECT_EQ(app.mpCommonRegion->value, 2);
    // State1 -> State2
    app.update(); 
    EXPECT_EQ(app.mpCommonRegion->value, 3);
    // State2 -> Final
    app.update();  
    // Final -> Exit
    EXPECT_TRUE(app.endAll());
}

// Test state caching functionality
TEST_F(StateMachineTest, StateCaching)
{
    app.init(TestState::Initial);
    EXPECT_EQ(app.mpCommonRegion->value, 1);

    // Transition to State1 with caching enabled
    app.changeState(TestState::State1, true);
    app.update();
    EXPECT_EQ(app.mpCommonRegion->value, 2);

    // Transition to State2
    app.changeState(TestState::State2);
    app.update();
    EXPECT_EQ(app.mpCommonRegion->value, 3);

    // Return to cached State1
    app.changeState(TestState::State1);
    app.update();
    EXPECT_EQ(app.mpCommonRegion->value, 2);
}

// Test adding duplicate states
TEST_F(StateMachineTest, DuplicateStateAddition)
{
    // Try to add an already existing state
#ifndef NDEBUG
    EXPECT_DEATH(app.addState<InitialState>(TestState::Initial), "");
#else
    EXPECT_NO_THROW(app.addState<InitialState>(TestState::Initial));
#endif
}

// Test invalid state transitions
TEST_F(StateMachineTest, InvalidStateTransition)
{
    app.init(TestState::Initial);

    // Test transition to non-existent state
    EXPECT_DEATH(app.changeState(static_cast<TestState>(999)), "");
}

// Test state reset functionality
class ResetTestState : public ec2s::State<TestState, TestCommonRegion>
{
    GEN_STATE_CTOR_ONLY(ResetTestState, TestState, TestCommonRegion);

public:
    virtual ~ResetTestState() override
    {
    }

    void init() override
    {
        getCommonRegion()->value += 1;
    }
    void update() override
    {
        resetState();
    }
};

// Test application exit functionality
TEST_F(StateMachineTest, ApplicationExit)
{
    app.init(TestState::Initial);

    // Run through all states until Final
    while (!app.endAll())
    {
        app.update();
    }

    EXPECT_TRUE(app.endAll());
}

// Test common region accessibility
TEST_F(StateMachineTest, CommonRegionAccess)
{
    app.init(TestState::Initial);
    EXPECT_NE(app.mpCommonRegion.get(), nullptr);
    EXPECT_EQ(app.mpCommonRegion->value, 1);
}