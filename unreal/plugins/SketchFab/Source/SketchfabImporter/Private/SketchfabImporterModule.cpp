#include "Modules/ModuleManager.h"

class FSketchfabImporterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FSketchfabImporterModule, SketchfabImporter)
