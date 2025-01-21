/*****************************************************************//**
 * @file   Application.hpp
 * @brief  header file of Application class
 * 
 * @author ichi-raven
 * @date   November 2024
 *********************************************************************/

//! forward declaration
namespace ec2s
{
    template <typename Key, typename CommonRegion>
    class Application;

    template <typename Key, typename CommonRegion>
    class State;
}  // namespace ec2s

#ifndef EC2S_EC2S_APPLICATION_HPP_
#define EC2S_EC2S_APPLICATION_HPP_

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * @brief  constructor, destructor, init, update auto-generated macros for State class
 */
#define GEN_STATE(STATE_TYPE, KEY_TYPE, COMMONREGION_TYPE)                                                                   \
public:                                                                                                                      \
    STATE_TYPE(ec2s::Application<KEY_TYPE, COMMONREGION_TYPE>* application, std::shared_ptr<COMMONREGION_TYPE> commonRegion) \
        : State(application, commonRegion)                                                                                   \
    {                                                                                                                        \
    }                                                                                                                        \
    virtual ~STATE_TYPE() override;                                                                                          \
    virtual void init() override;                                                                                            \
    virtual void update() override;                                                                                          \
                                                                                                                             \
private:

/**
 * @brief  constructor only auto-generated macros for State class
 */
#define GEN_STATE_CTOR_ONLY(STATE_TYPE, KEY_TYPE, COMMONREGION_TYPE)                                                         \
public:                                                                                                                      \
    STATE_TYPE(ec2s::Application<KEY_TYPE, COMMONREGION_TYPE>* application, std::shared_ptr<COMMONREGION_TYPE> commonRegion) \
        : State(application, commonRegion)                                                                                   \
    {                                                                                                                        \
    }                                                                                                                        \
                                                                                                                             \
private:

namespace ec2s
{
    /**
     * @brief  represents a state in a state machine (also called a scene)
     * @tparam Key_t type of State key (identifies the State)
     * @tparam CommonRegion_t type of region shared by all States in the Application
     */
    template <typename Key_t, typename CommonRegion_t>
    class State
    {
    private:
        using Application_t = Application<Key_t, CommonRegion_t>;

    public:

        State() = delete;

        /** 
         * @brief  constructor (should normally be generated automatically by the macro GEN_STATE)
         *  
         * @param pApplication pointer to Application instance
         * @param pCommonRegion pointer to CommonRegion of Application
         */
        State(Application_t* pApplication, std::shared_ptr<CommonRegion_t> pCommonRegion)
            : mpApplication(pApplication)
            , mpCommonRegion(pCommonRegion)
            , mStateChanged(false)
        {
        }

        /**
         * @brief  destructor
         */
        virtual ~State(){};

        /** 
         * @brief  State initialization function (automatically called by Application)
         *  
         */
        virtual void init() = 0;

        /** 
         * @brief  State update function (automatically called by Application)
         *  
         */
        virtual void update() = 0;

    protected:

        /** 
         * @brief  transition to the specified State
         *  
         * @param dstStateKey key of the destination State
         * @param cachePrevState whether the current state of the state should be retained (e.g., pause screen, etc.)
         */
        void changeState(const Key_t& dstStateKey, bool cachePrevState = false)
        {
            mStateChanged = true;
            mpApplication->changeState(dstStateKey, cachePrevState);
        }

        /** 
         * @brief  reset the current State state
         *  
         */
        void resetState()
        {
            init();
        }

        /** 
         * @brief  terminate the entire Application
         *  
         */
        void exitApplication()
        {
            mpApplication->dispatchEnd();
        }

        /** 
         * @brief  get CommonRegion reference
         *  
         * @return reference to shared_ptr<CommonRegion>
         */
        const std::shared_ptr<CommonRegion_t>& getCommonRegion() const
        {
            return mpCommonRegion;
        }

        /** 
         * @brief  get CommonRegion reference (shortened version)
         *  
         * @return reference to shared_ptr<CommonRegion>
         */
        const std::shared_ptr<CommonRegion_t>& common() const
        {
            return mpCommonRegion;
        }

    private:
        //! shared pointer to CommonRegion
        std::shared_ptr<CommonRegion_t> mpCommonRegion;
        //! pointer to application
        Application_t* mpApplication;
        //! flag indicating that a State change has occurred
        bool mStateChanged;
    };

    /**
     * @brief  representing the entire application, where State is registered and used
     * @tparam Key_t type of State key (identifies the State)
     * @tparam CommonRegion_t type of region shared by all States in the Application
     */
    template <typename Key_t, typename CommonRegion_t>
    class Application
    {
    private:
        using State_t   = std::shared_ptr<State<Key_t, CommonRegion_t>>;
        using Factory_t = std::function<State_t()>;

    public:
        /** 
         * @brief  constructor
         *  
         */
        Application()
            : mpCommonRegion(std::make_shared<CommonRegion_t>())
            , mEndFlag(false)
            , mChangedFlag(false)
        {
        }

        // Noncopyable, Nonmoveable
        Application(const Application&)            = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&)                 = delete;
        Application& operator=(Application&&)      = delete;

        /** 
         * @brief  destructor
         *  
         */
        ~Application()
        {
        }

        /** 
         * @brief  initialize Application and set it to start from the specified State
         *  
         * @param firstStateKey start with this State
         */
        void init(const Key_t& firstStateKey)
        {
            mFirstStateKey = firstStateKey;
            mEndFlag       = false;

            mCurrent.first  = mFirstStateKey.value();
            mCurrent.second = mStatesFactory[mFirstStateKey.value()]();
            mCurrent.second->init();
        }

        /** 
         * @brief  update the entire Application (called in the main loop)
         *  
         */
        void update()
        {
            if (mChangedFlag)
            {
                mCurrent.second->init();
                mChangedFlag = false;
            }

            mCurrent.second->update();
        }

        /** 
         * @brief  Add a State (can be transitioned from another State)
         * 
         * @tparam InheritedState type of State to add
         * @param key key of State to add
         */
        template <typename InheritedState>
        void addState(const Key_t& key)
        {
            if (mStatesFactory.find(key) != mStatesFactory.end())
            {
#ifndef NDEBUG
                assert(!"this key already exist!");
#endif  //NDEBUG

                return;
            }

            mStatesFactory.emplace(key,
                                   [&]()
                                   {
                                       auto&& m = std::make_shared<InheritedState>(this, mpCommonRegion);
                                       return m;
                                   });

            if (!mFirstStateKey)
            {
                mFirstStateKey = key;
            }
        }

        /** 
         * @brief  changes the current State to the specified State (usually called from the State side)
         *  
         * @param dstStateKey key of the destination state
         * @param cachePrevState whether the current state of the state should be retained (e.g., pause screen, etc.)
         */
        void changeState(const Key_t& dstStateKey, const bool cachePrevState = false)
        {
            assert(mStatesFactory.find(dstStateKey) != mStatesFactory.end() || !"invalid change dst state!");

            if (cachePrevState)
            {
                mCache = mCurrent;
            }

            if (mCache && dstStateKey == mCache.value().first)
            {
                mCurrent = mCache.value();
                mCache   = std::nullopt;
            }
            else
            {
                mCurrent.first  = dstStateKey;
                mCurrent.second = mStatesFactory[dstStateKey]();
            }

            mChangedFlag = true;
        }

        /** 
         * @brief  notify the end of the Application (turn on the end flag)
         *  
         */
        void dispatchEnd()
        {
            mEndFlag = true;
        }

        /** 
         * @brief  judges whether the Application is terminated (used to determine the end of the main loop)
         *  
         * @return whether the Application is ended
         */
        bool endAll()
        {
            return mEndFlag;
        }

    public:
        //! CommonRegion
        std::shared_ptr<CommonRegion_t> mpCommonRegion;

    private:
        //! hashmap with State's key and its instance's factory
        std::unordered_map<Key_t, Factory_t> mStatesFactory;
        //! key/instance pairs for the current State
        std::pair<Key_t, State_t> mCurrent;
        //! cache of previous State (used if the cachePrevState flag was on)
        std::optional<std::pair<Key_t, State_t>> mCache;
        //! key for the starting State
        std::optional<Key_t> mFirstStateKey;
        //! flag indicating whether the Application should be terminated
        bool mEndFlag;
        //! flag indicating whether a State transition has occurred
        bool mChangedFlag;
    };

};  // namespace ec2s

#endif
