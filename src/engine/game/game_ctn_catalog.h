#pragma once

#include <vector>

#include "engine/core/cmw_nod.h"
#include "engine/game/game_catalog_assets.h"
#include "engine/game/game_identifier.h"
class CGameCtnCollection;
class CGameCtnCatalog;
class CGameCtnDecoration;
class CGameSkin;
struct CSystemPackDesc;

class CGameCtnArticle : public CMwNod {
public:
    CGameCtnArticle(void);
    ~CGameCtnArticle(void) override;

    void Purge(void);
    static void PurgeAllForce(void);
    static void PrepareCacheForLoading(
            const SGameCtnIdentifier &identifier,
            CSystemPackDesc *packDesc);

    void SetIdentifier(SGameCtnIdentifier identifier);
    const SGameCtnIdentifier &Identifier() const;
    void SetSortIndex(uint16_t sortIndex);
    uint16_t SortIndex() const;
    void SetAssetSource(GameCatalogAssetSourceHandle source);
    GameCatalogAssetKind AssetKind() const;

protected:
    CMwNod *GetNod(int loadNow);

private:
    friend class CGameCtnCatalog;
    friend class CGameCtnChapter;
    friend class CGameCtnDecoration;

    bool IsDecorationAsset() const {
        return AssetKind() == GameCatalogAssetKind::Decoration;
    }
    CGameCtnCatalog *catalog_ = nullptr;
    SGameCtnIdentifier identifier_;
    uint16_t sortIndex_ = 0u;
    GameCatalogAssetSourceHandle source_;
    CMwNodRef<CMwNod> cachedNode_;
};

class CGameCtnChapter : public CMwNod {
public:
    CGameCtnChapter(void);
    ~CGameCtnChapter(void) override;
    void ClearAll(void);
    void AddArticle(CGameCtnArticle *article);
    CGameCtnCollection *GetNod(void);
    CGameCtnArticle *GetArticle(SGameCtnIdentifier identifier);
    CGameCtnArticle *GetDecorationIfUnique(void);

    void SetCollectionId(CMwId collectionId);
    const CMwId &CollectionId() const;
    void SetCollectionSource(GameCatalogAssetSourceHandle source);
    bool HasCollectionSource() const;

private:
    friend class CGameCtnCatalog;

    CGameCtnCatalog *catalog_ = nullptr;
    std::vector<CMwNodRef<CGameCtnArticle>> articles_;
    GameCatalogAssetSourceHandle collectionSource_;
    CMwId collectionId_;
};

class CGameCtnCatalog : public CMwNod {
public:
    CGameCtnCatalog(void);
    ~CGameCtnCatalog(void) override;
    void AddChapter(CGameCtnChapter *chapter);
    void AddArticle(CGameCtnArticle *article);
    CGameCtnChapter *GetChapter(const CMwId &id);
    CGameCtnArticle *GetArticleFromIdent(const SGameCtnIdentifier &ident);

private:
    friend class CGameCtnArticle;

    void TrackLoaded(CGameCtnArticle &article);
    void ForgetLoaded(CGameCtnArticle &article);
    void PurgeLoadedArticles();
    void PrepareDecorationLoad(const SGameCtnIdentifier &identifier,
                               CSystemPackDesc *packDesc);

    std::vector<CMwNodRef<CGameCtnChapter>> chapters_;
    std::vector<CGameCtnArticle *> loadedArticles_;
    SGameCtnIdentifier decorationCacheIdentifier_;
    CMwNodRef<CSystemPackDesc> decorationCacheSkin_;
};
