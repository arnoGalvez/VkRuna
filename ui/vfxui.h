// Copyright (c) 2021 Arno Galvez

#pragma once

#include "platform/defines.h"
#include "ui/Controller.h"
#include "ui/Ui.h"

#include <memory>
#include <vector>

namespace vkRuna
{
class VFXUIManager
{
	NO_COPY_NO_ASSIGN( VFXUIManager )

   public:
	VFXUIManager();
	~VFXUIManager();

	void Init();
	void Shutdown();
};

extern VFXUIManager g_vfxuiManager;

class VFXUI : public UIElement
{
   public:
	void		Draw() final;
	const char *GetName() final;
	void		SetName( const char *name ) final {}

   private:
	void DrawPipelineController( PipelineController &pipCtrl, const char *imKey );
	void DrawGraphicsPipelineController( VFXController &	 vfxCtrl,
										 PipelineController &pipCtrl,
										 const char *		 pipelineName,
										 const char *		 imKey );
	void DrawComputePipelineController( PipelineController &pipCtrl, const char *pipelineName, const char *imKey );

   private:
	std::vector< VFXController > m_vfxControllers;
};

} // namespace vkRuna
