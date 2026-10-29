#pragma once

#include <stddef.h>
#include "Thread.h"
#include "GameSound.h"
#include "RedEngineFL/Interface/RedBitmapElement.h"
#include "RedEngineFL/Interface/RedInterfaceScreen.h"

// VTABLE: SWBF2 0x007ae400
class ScopeDisplay : public Thread
{
public:

    class BinocularSound
    {
    public:
        BinocularSound();
        ~BinocularSound();

        void Enter();
        void Exit();
        void ZoomIn();
        void ZoomOut();
        void ZoomStop();
        void Update();
        void SetAmbientID(const char* pName);
        void SetZoomInID(const char* pName);
        void SetZoomOutID(const char* pName);

        GameSoundControllable mAmbient;
        GameSound mAmbientProps;
        GameSoundControllable mZoomIn;
        GameSound mZoomInProps;
        GameSoundControllable mZoomOut;
        GameSound mZoomOutProps;
    };

    ScopeDisplay();
    virtual ~ScopeDisplay();

    // SYNTHETIC: SWBF2 0x00633940
    // ScopeDisplay::`scalar deleting destructor'

    virtual bool Update(float fDeltaTime);

    void Init();
    void Bind(unsigned int uJoystick);
    void Show();
    void Hide();

    static void Create();

    static void InitAll();
    static void HideAll();

    // OpenSWBF2: sizes the overlay for the current screen (a round lens in a
    // centred 4:3 box, bars at the sides on wider screens).
    void UpdateLayout();

    int mJoystickIndex;
    RedScreenGroupElement mGroupWeaponScope;
    RedBitmapElement mBitmapWeaponScope[4];
    RedBitmapElement mBitmapScopeBar[2];
    unsigned int mLayoutW;
    unsigned int mLayoutH;
    unsigned int mScopeTexture;
    float mPrevZoom;
    bool mWeaponScope;
    bool mInBinocularMode;
    BinocularSound mBinocularSound;
    bool mIsZooming;

    static ScopeDisplay* sInstance[1];

    static ScopeDisplay* Instance(unsigned int uiIndex) { return uiIndex < 1 ? sInstance[uiIndex] : 0; }
    bool IsActive() const { return mInBinocularMode; }

    static bool sCanRender;
};
 
 
 
 
 
 
 
 
 
 
