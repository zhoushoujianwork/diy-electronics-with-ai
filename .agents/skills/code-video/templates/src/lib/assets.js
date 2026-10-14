// 图片素材：只用目标项目仓库里已有的截图和 logo，放在 src/assets/。
// 只在浏览器端调用 loadAssets()；draw 里只读 assets，不写。
export const assets = {};

const FILES = {
  // logo: '../assets/logo.png',
};

export async function loadAssets() {
  await Promise.all(
    Object.entries(FILES).map(async ([key, rel]) => {
      const img = new Image();
      img.src = new URL(rel, import.meta.url).href;
      await img.decode();
      assets[key] = img;
    }),
  );
}
