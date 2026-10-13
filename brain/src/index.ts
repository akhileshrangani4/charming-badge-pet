import type { Env } from "./pet";
export { Pet } from "./pet";

function sameSecret(a: string, b: string) {
  if (!a || a.length !== b.length) return false;
  let diff = 0; for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

export default {
  async fetch(req: Request, env: Env): Promise<Response> {
    const url = new URL(req.url);
    const pet = env.PET.get(env.PET.idFromName("main"));
    if (url.pathname === "/badge") return pet.fetch(req);
    const appRoutes = ["/chat", "/state", "/settings", "/networks", "/device-token/rotate", "/compact"];
    if (appRoutes.includes(url.pathname)) {
      if (!sameSecret(req.headers.get("X-App-Secret") ?? "", env.APP_SHARED_SECRET)) return new Response("unauthorized", { status: 401 });
      return pet.fetch(req);
    }
    return new Response("not found", { status: 404 });
  },
};
