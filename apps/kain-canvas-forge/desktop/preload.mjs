import { contextBridge } from "electron";

contextBridge.exposeInMainWorld("canvasForgeDesktop", {
  runtime: "electron"
});

