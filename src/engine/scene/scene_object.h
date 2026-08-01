#pragma once

#include <memory>

#include "engine/core/cmw_nod.h"
#include "engine/core/mw_id.h"
class CHmsSoundSource;
struct CPlugSound;
struct CScene;

class CMotion : public CMwNod {
public:
    virtual void Stop(void);
    virtual void Play(void);
    virtual void Pause(void);
};

class CSceneMessageHandler : public CMwNod {
public:
    CSceneMessageHandler(void);
    ~CSceneMessageHandler(void) override;
};

class CSceneObject : public CMwNod {
public:
    CSceneObject(void);
    ~CSceneObject(void) override;

    CMotion *GetMotion(void) const;
    virtual void OnEnterScene(void);
    virtual void OnLeaveScene(void);
    void RemoveFromScene(void);

    CScene *scene = nullptr;
    CMwId id;
    CMwNodRef<CMotion> motion;
    bool ownedBySceneMobilLink = false;
};

class CScenePoc : public CSceneObject {
public:
    CScenePoc(void);
    ~CScenePoc(void) override;
};

class CSceneSoundSource : public CScenePoc {
public:
    CSceneSoundSource(void);
    ~CSceneSoundSource(void) override;

    void Stop(void);
    void Play(void);
    void CreateSound(CPlugSound *sound);
    void AttachSound(CHmsSoundSource &inlineSound);

private:
    friend class CSceneVehicleCar;

    std::unique_ptr<CHmsSoundSource> ownedSound;
    CHmsSoundSource *sound = nullptr;
};
