#pragma once

#include <memory>
#include <optional>
#include <string>

struct CPlugFilePack;

struct DecorationTerrainModifierRemapPath {
    std::string target;
};

class SkinMaterialRemapCatalog {
public:
    SkinMaterialRemapCatalog();
    ~SkinMaterialRemapCatalog();

    SkinMaterialRemapCatalog(const SkinMaterialRemapCatalog &) = delete;
    SkinMaterialRemapCatalog &operator=(
            const SkinMaterialRemapCatalog &) = delete;

    bool Load(const CPlugFilePack &pack);
    std::optional<DecorationTerrainModifierRemapPath>
    FindDecorationTerrainModifier(
            const std::string &sourceMaterialPath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
