
const SKETCHFAB_API = "https://api.sketchfab.com/v3";

// Sort options for randomization effect
const SORT_OPTIONS = ['-relevance', '-likeCount', '-viewCount', '-publishedAt', '-createdAt'];

export interface SketchfabModel {
    uid: string;
    name: string;
    description: string;
    thumbnails: {
        images: {
            url: string;
            width: number;
            height: number;
        }[];
    };
    user: {
        username: string;
        displayName: string;
    };
    viewerUrl: string;
}

export interface SketchfabSearchResult {
    models: SketchfabModel[];
    totalCount: number;
    nextCursor: string | null;
    prevCursor: string | null;
}

export interface SearchOptions {
    token?: string;
    count?: number;      // Number of results per page (max 24)
    cursor?: string;     // Pagination cursor
    randomize?: boolean; // Shuffle sort order for variety
}

export const searchSketchfab = async (query: string, options: SearchOptions = {}): Promise<SketchfabSearchResult> => {
    if (!query) return { models: [], totalCount: 0, nextCursor: null, prevCursor: null };

    const { token, count = 24, cursor, randomize = false } = options;

    // Pick a random sort for variety if randomize is enabled
    const sortBy = randomize
        ? SORT_OPTIONS[Math.floor(Math.random() * SORT_OPTIONS.length)]
        : '-relevance';

    // Build URL with pagination
    let url = `${SKETCHFAB_API}/search?type=models&q=${encodeURIComponent(query)}&downloadable=true&sort_by=${sortBy}&count=${count}`;
    if (cursor) {
        url += `&cursor=${cursor}`;
    }

    const headers: Record<string, string> = {};
    if (token) {
        headers['Authorization'] = `Token ${token}`;
    }
    const response = await fetch(url, { headers });
    if (!response.ok) throw new Error("Sketchfab search failed");

    const data = await response.json();

    // Extract cursors from the response
    const nextCursor = data.cursors?.next || null;
    const prevCursor = data.cursors?.previous || null;

    return {
        models: data.results.map((r: any) => ({
            uid: r.uid,
            name: r.name,
            description: r.description,
            thumbnails: r.thumbnails,
            user: r.user,
            viewerUrl: r.viewerUrl
        })),
        totalCount: data.totalResults || data.results.length,
        nextCursor,
        prevCursor
    };
};

/**
 * Note: Downloading from Sketchfab via API usually requires OAuth token.
 * For this integration, we will try to get the download URL.
 * If authentication is needed, we might need a prompt or a token.
 */
export const getSketchfabDownloadUrl = async (uid: string, token?: string): Promise<string | null> => {
    const url = `${SKETCHFAB_API}/models/${uid}/download`;
    const headers: Record<string, string> = {};
    if (token) {
        headers['Authorization'] = `Token ${token}`;
    }

    const response = await fetch(url, { headers });
    if (!response.ok) {
        if (response.status === 401) {
            throw new Error("Sketchfab authentication required for download");
        }
        return null;
    }

    const data = await response.json();
    // Prefer glb if available, otherwise gltf
    if (data.glb) return data.glb.url;
    if (data.gltf) return data.gltf.url;

    return null;
};
