// ëOï˚êÈåæÇÃÇ›
namespace ec2s
{
    template <typename Key, typename CommonRegion>
    class Application;

    template <typename Key, typename CommonRegion>
    class Scene;
}  // namespace ec2s

#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#define GEN_SCENE(SCENE_TYPE, KEY_TYPE, COMMONREGION_TYPE)                                                                   \
public:                                                                                                                      \
    SCENE_TYPE(ec2s::Application<KEY_TYPE, COMMONREGION_TYPE>* application, std::shared_ptr<COMMONREGION_TYPE> commonRegion) \
        : Scene(application, commonRegion)                                                                                   \
    {                                                                                                                        \
    }                                                                                                                        \
    virtual ~SCENE_TYPE() override;                                                                                          \
    virtual void init() override;                                                                                            \
    virtual void update() override;                                                                                          \
                                                                                                                             \
private:

namespace ec2s
{

    template <typename Key_t, typename CommonRegion>
    class Scene
    {
    private:  
        using Application_t = Application<Key_t, CommonRegion>;

    public: 
        Scene() = delete;

        Scene(Application_t* pApplication, std::shared_ptr<CommonRegion> pCommonRegion)
            : mpApplication(pApplication)
            , mpCommonRegion(pCommonRegion)
        {
        }

        virtual ~Scene(){};

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
        void changeScene(const Key_t& dstSceneKey, bool cachePrevScene = false)
        {
            mSceneChanged = true;
            mpApplication->changeScene(dstSceneKey, cachePrevScene);
        }

        void resetScene()
        {
            initAll();
        }

        void exitApplication()
        {
            mpApplication->dispatchEnd();
        }

        const std::shared_ptr<CommonRegion>& getCommonRegion() const
        {
            return mpCommonRegion;
        }

    private: 
        std::shared_ptr<CommonRegion> mpCommonRegion;
        Application_t* mpApplication;  

        bool mSceneChanged;
    };

    template <typename Key_t, typename CommonRegion>
    class Application
    {
    private:  
        using Scene_t   = std::shared_ptr<Scene<Key_t, CommonRegion>>;
        using Factory_t = std::function<Scene_t()>;

    public:
        template <typename T>
        Application()
            : mpCommonRegion(std::make_shared<CommonRegion>())
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

        void init(const Key_t& firstSceneKey)
        {
            mFirstSceneKey = firstSceneKey;
            mEndFlag       = false;

            assert(mFirstSceneKey);

            mCurrent.first  = mFirstSceneKey.value();
            mCurrent.second = mScenesFactory[mFirstSceneKey.value()]();
        }

        void update()
        {
            mCurrent.second->updateAll();
        }

        template <typename InheritedScene>
        void addScene(const Key_t& key)
        {
            if (mScenesFactory.find(key) != mScenesFactory.end())
            {
#ifndef NDEBUG
                assert(!"this key already exist!");
#endif  //NDEBUG

                return;
            }

            mScenesFactory.emplace(key,
                                   [&]()
                                   {
                                       auto&& m = std::make_shared<InheritedScene>(this, mpCommonRegion);
                                       m->initAll();

                                       return m;
                                   });

            if (!mFirstSceneKey)
            {
                mFirstSceneKey = key;
            }
        }

        void changeScene(const Key_t& dstSceneKey, const bool cachePrevScene = false)
        {
            assert(mScenesFactory.find(dstSceneKey) != mScenesFactory.end());

            if (cachePrevScene)
            {
                mCache = mCurrent;
            }

            if (mCache && dstSceneKey == mCache.value().first)
            {
                mCurrent = mCache.value();
                mCache   = std::nullopt;
            }
            else
            {
                mCurrent.first  = dstSceneKey;
                mCurrent.second = mScenesFactory[dstSceneKey]();
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
        std::shared_ptr<CommonRegion> mpCommonRegion;

    private:
        std::unordered_map<Key_t, Factory_t> mScenesFactory;
        std::pair<Key_t, Scene_t> mCurrent;
        std::optional<std::pair<Key_t, Scene_t>> mCache;
        std::optional<Key_t> mFirstSceneKey; 
        bool mEndFlag;
    };

};  // namespace ec2s