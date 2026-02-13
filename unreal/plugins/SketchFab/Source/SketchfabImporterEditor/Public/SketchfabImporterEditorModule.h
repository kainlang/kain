#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSketchfabImporterEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OnSketchfabBrowserClicked();
	void OnCombineMeshesClicked();
	
	TSharedPtr<class FUICommandList> PluginCommands;
};
