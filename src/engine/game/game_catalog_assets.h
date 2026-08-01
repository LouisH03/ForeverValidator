#pragma once

#include <memory>

struct CMwNod;

enum class GameCatalogAssetKind {
    Collection,
    Decoration,
    BlockInfo,
    Other,
};

struct GameCatalogLoadOptions {
    bool includeSiblingShaders = false;
};

class GameCatalogAssetSource {
public:
    virtual ~GameCatalogAssetSource() = default;

    virtual GameCatalogAssetKind Kind() const = 0;
    virtual CMwNod *LoadedNode() const = 0;
    virtual CMwNod *LoadNode(const GameCatalogLoadOptions &options,
                             CMwNod &requester) = 0;
};

using GameCatalogAssetSourceHandle =
        std::shared_ptr<GameCatalogAssetSource>;
