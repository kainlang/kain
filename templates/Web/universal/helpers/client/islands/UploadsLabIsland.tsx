import { h } from "preact";
import { useMemo, useState } from "preact/hooks";

type UploadResponse = {
  ok: boolean;
  file?: {
    href: string;
    filename: string;
    byte_length: number;
    content_type: string;
  };
  error?: string;
};

async function fileToDataUrl(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result || ""));
    reader.onerror = () => reject(reader.error || new Error("file read failed"));
    reader.readAsDataURL(file);
  });
}

async function uploadFile(file: File): Promise<UploadResponse> {
  const content_base64 = await fileToDataUrl(file);
  const response = await fetch("/api/uploads", {
    method: "POST",
    headers: { "content-type": "application/json", accept: "application/json" },
    body: JSON.stringify({
      filename: file.name,
      content_type: file.type || "application/octet-stream",
      content_base64
    })
  });
  return (await response.json()) as UploadResponse;
}

export function UploadsLabIsland() {
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState<string | null>(null);
  const [uploads, setUploads] = useState<NonNullable<UploadResponse["file"]>[]>([]);

  const hint = useMemo(
    () => "Posts base64 payloads to /api/uploads and serves them from /uploads/* while the local server is running.",
    []
  );

  const onFile = async (file: File | null) => {
    if (!file || busy) return;
    try {
      setBusy(true);
      setStatus("uploading");
      const payload = await uploadFile(file);
      if (!payload.ok || !payload.file) {
        setStatus(payload.error || "upload failed");
        return;
      }
      setUploads((prev) => [payload.file!, ...prev].slice(0, 10));
      setStatus(`stored ${payload.file.filename}`);
    } catch (error) {
      setStatus((error as Error).message || "upload error");
    } finally {
      setBusy(false);
    }
  };

  return (
    <div class="kain-island kain-island-uploads">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Uploads</p>
        <h3 class="kain-island-title">Upload lab</h3>
        <p class="kain-island-copy">{hint}</p>
      </div>
      <div class="kain-uploads-body">
        <input
          type="file"
          disabled={busy}
          onChange={(event) => {
            const input = event.target as HTMLInputElement;
            void onFile(input.files?.[0] || null);
            input.value = "";
          }}
        />
        <p class="kain-island-status">{status || (busy ? "working" : "idle")}</p>
        <div class="kain-uploads-list">
          {uploads.map((entry, index) => (
            <article class="kain-uploads-item" key={index}>
              <a href={entry.href} target="_blank" rel="noreferrer">
                {entry.filename}
              </a>
              <p class="kain-uploads-meta">
                {[entry.content_type, `${Math.round(entry.byte_length / 1024)}kb`].join(" · ")}
              </p>
            </article>
          ))}
        </div>
      </div>
    </div>
  );
}

