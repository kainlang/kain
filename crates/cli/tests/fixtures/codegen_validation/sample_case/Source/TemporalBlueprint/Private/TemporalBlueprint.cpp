#include "Temporal.h"
#include "Modules/ModuleManager.h"

class FTemporalBlueprintModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FTemporalBlueprintModule, TemporalBlueprint)
