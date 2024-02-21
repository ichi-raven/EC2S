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

namespace ec2s
{

    template <typename Key_t, typename CommonRegion_t>
    class State
    {
    private:
        using Application_t = Application<Key_t, CommonRegion_t>;

    public:
        State() = delete;

        State(Application_t* pApplication, std::shared_ptr<CommonRegion_t> pCommonRegion)
            : mpApplication(pApplication)
            , mpCommonRegion(pCommonRegion)
            , mStateChanged(false)
        {
        }

        virtual ~State(){};

        virtual void init() = 0;

        virtual void update() = 0;

        inline void initAll()
        {
            init();
        }

        void updateAll()
        {
            update();
        }

    protected:
        void changeState(const Key_t& dstStateKey, bool cachePrevState = false)
        {
            mStateChanged = true;
            mpApplication->changeState(dstStateKey, cachePrevState);
        }

        void resetState()
        {
            initAll();
        }

        void exitApplication()
        {
            mpApplication->dispatchEnd();
        }

        const std::shared_ptr<CommonRegion_t>& getCommonRegion() const
        {
            return mpCommonRegion;
        }

        const std::shared_ptr<CommonRegion_t>& get() const
        {
            return mpCommonRegion;
        }

    private:
        CommonRegion_t* mpCommonRegion;
        Application_t* mpApplication;

        bool mStateChanged;
    };

    template <typename Key_t, typename CommonRegion_t>
    class Application
    {
    private:
        using State_t   = std::shared_ptr<State<Key_t, CommonRegion_t>>;
        using Factory_t = std::function<State_t()>;

    public:
        Application()
            : mpCommonRegion(std::make_shared<CommonRegion_t>())
            , mEndFlag(false)
        {
        }

        // Noncopyable, Nonmoveable
        Application(const Application&)            = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&)                 = delete;
        Application& operator=(Application&&)      = delete;

        ~Application()
        {
        }

        void init(const Key_t& firstStateKey)
        {
            mFirstStateKey = firstStateKey;
            mEndFlag       = false;

            assert(mFirstStateKey);

            mCurrent.first  = mFirstStateKey.value();
            mCurrent.second = mStatesFactory[mFirstStateKey.value()]();
        }

        void update()
        {
            mCurrent.second->updateAll();
        }

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
                                       m->initAll();

                                       return m;
                                   });

            if (!mFirstStateKey)
            {
                mFirstStateKey = key;
            }
        }

        void changeState(const Key_t& dstStateKey, const bool cachePrevState = false)
        {
            assert(mStatesFactory.find(dstStateKey) != mStatesFactory.end());

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
        }

        void dispatchEnd()
        {
            mEndFlag = true;
        }

        bool endAll()
        {
            return mEndFlag;
        }

    public:
        std::shared_ptr<CommonRegion_t> mpCommonRegion;

    private:
        std::unordered_map<Key_t, Factory_t> mStatesFactory;
        std::pair<Key_t, State_t> mCurrent;
        std::optional<std::pair<Key_t, State_t>> mCache;
        std::optional<Key_t> mFirstStateKey;
        bool mEndFlag;
    };

};  // namespace ec2s

#endif
