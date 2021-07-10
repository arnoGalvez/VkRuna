// Copyright (c) 2021 Arno Galvez

#pragma once

namespace vkRuna {
namespace Os {
class Window;
}
namespace render {
struct gpuCmd_t;
class Backend {
 public:
  static Backend &GetInstance();

  virtual void Init() = 0;
  virtual void Shutdown() = 0;

  virtual void ExecuteCommands(int preRenderCount,
                               const gpuCmd_t *preRenderCmds,
                               int renderCmdCount,
                               const gpuCmd_t *renderCmds) = 0;
  virtual void Present() = 0;

  virtual void OnWindowSizeChanged();
};
}  // namespace render
}  // namespace vkRuna