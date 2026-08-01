#pragma once

struct CPlugFilePack;
class InstalledPackAssetRepository;
class StaticSolidArchiveReferenceCatalog;

struct InstalledPackAssetRepositoryAccess {
    static const CPlugFilePack *Pack(
            InstalledPackAssetRepository &repository);
    static StaticSolidArchiveReferenceCatalog &StaticSolidReferences(
            InstalledPackAssetRepository &repository);
};
