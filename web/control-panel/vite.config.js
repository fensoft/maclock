import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";
import { viteSingleFile } from "vite-plugin-singlefile";

export default defineConfig({
  base: "./",
  plugins: [vue(), viteSingleFile()],
  define: {
    __VUE_OPTIONS_API__: false,
    __VUE_PROD_DEVTOOLS__: false,
    __VUE_PROD_HYDRATION_MISMATCH_DETAILS__: false,
  },
  build: {
    target: "es2018",
    cssCodeSplit: false,
    assetsInlineLimit: 100000000,
    reportCompressedSize: true,
  },
  server: {
    host: "127.0.0.1",
    port: 4174,
  },
});
