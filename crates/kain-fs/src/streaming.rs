use std::fs::{self, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::Path;

use crate::{FsError, FsResult};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FsChunk {
    pub index: u64,
    pub offset: u64,
    pub bytes: Vec<u8>,
}

impl FsChunk {
    pub fn len(&self) -> u64 {
        self.bytes.len() as u64
    }

    pub fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }
}

pub fn read_byte_range(path: impl AsRef<Path>, offset: u64, length: usize) -> FsResult<Vec<u8>> {
    let path = path.as_ref();
    let mut file =
        fs::File::open(path).map_err(|error| FsError::from_io("read_byte_range", path, error))?;
    file.seek(SeekFrom::Start(offset))
        .map_err(|error| FsError::from_io("read_byte_range", path, error))?;
    let mut buffer = vec![0_u8; length];
    let read = file
        .read(&mut buffer)
        .map_err(|error| FsError::from_io("read_byte_range", path, error))?;
    buffer.truncate(read);
    Ok(buffer)
}

pub fn read_text_range(path: impl AsRef<Path>, offset: u64, length: usize) -> FsResult<String> {
    let bytes = read_byte_range(path.as_ref(), offset, length)?;
    String::from_utf8(bytes).map_err(|error| {
        FsError::new(
            "read_text_range",
            path.as_ref(),
            crate::FsErrorKind::InvalidInput,
            error.to_string(),
        )
    })
}

pub fn write_bytes_at(path: impl AsRef<Path>, offset: u64, bytes: &[u8]) -> FsResult<()> {
    let path = path.as_ref();
    if let Some(parent) = path.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)
                .map_err(|error| FsError::from_io("write_bytes_at", parent, error))?;
        }
    }
    let mut file = OpenOptions::new()
        .create(true)
        .write(true)
        .open(path)
        .map_err(|error| FsError::from_io("write_bytes_at", path, error))?;
    file.seek(SeekFrom::Start(offset))
        .map_err(|error| FsError::from_io("write_bytes_at", path, error))?;
    file.write_all(bytes)
        .map_err(|error| FsError::from_io("write_bytes_at", path, error))
}

pub fn write_text_at(path: impl AsRef<Path>, offset: u64, text: &str) -> FsResult<()> {
    write_bytes_at(path, offset, text.as_bytes())
}

pub fn stream_file_chunks(path: impl AsRef<Path>, chunk_size: usize) -> FsResult<Vec<FsChunk>> {
    let chunk_size = chunk_size.max(1);
    let path = path.as_ref();
    let mut file = fs::File::open(path)
        .map_err(|error| FsError::from_io("stream_file_chunks", path, error))?;
    let mut chunks = Vec::new();
    let mut offset = 0_u64;
    let mut index = 0_u64;
    loop {
        let mut buffer = vec![0_u8; chunk_size];
        let read = file
            .read(&mut buffer)
            .map_err(|error| FsError::from_io("stream_file_chunks", path, error))?;
        if read == 0 {
            break;
        }
        buffer.truncate(read);
        chunks.push(FsChunk {
            index,
            offset,
            bytes: buffer,
        });
        offset += read as u64;
        index += 1;
    }
    Ok(chunks)
}

pub fn copy_file_streaming(
    source: impl AsRef<Path>,
    destination: impl AsRef<Path>,
    chunk_size: usize,
) -> FsResult<u64> {
    let source = source.as_ref();
    let destination = destination.as_ref();
    if let Some(parent) = destination.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)
                .map_err(|error| FsError::from_io("copy_file_streaming", parent, error))?;
        }
    }
    let mut input = fs::File::open(source).map_err(|error| {
        FsError::from_two_path_io("copy_file_streaming", source, destination, error)
    })?;
    let mut output = fs::File::create(destination).map_err(|error| {
        FsError::from_two_path_io("copy_file_streaming", source, destination, error)
    })?;
    let mut copied = 0_u64;
    let mut buffer = vec![0_u8; chunk_size.max(1)];
    loop {
        let read = input.read(&mut buffer).map_err(|error| {
            FsError::from_two_path_io("copy_file_streaming", source, destination, error)
        })?;
        if read == 0 {
            break;
        }
        output.write_all(&buffer[..read]).map_err(|error| {
            FsError::from_two_path_io("copy_file_streaming", source, destination, error)
        })?;
        copied += read as u64;
    }
    Ok(copied)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{read_text, write_text};

    #[test]
    fn ranges_chunks_and_streaming_copy_work() {
        let temp = tempfile::tempdir().expect("tempdir");
        let source = temp.path().join("source.txt");
        let dest = temp.path().join("nested").join("dest.txt");
        write_text(&source, "abcdef").expect("write");

        assert_eq!(read_text_range(&source, 1, 3).expect("range"), "bcd");
        write_text_at(&source, 3, "XYZ").expect("write at");
        assert_eq!(read_text(&source).expect("read"), "abcXYZ");

        let chunks = stream_file_chunks(&source, 2).expect("chunks");
        assert_eq!(chunks.len(), 3);
        assert_eq!(chunks[1].offset, 2);

        assert_eq!(copy_file_streaming(&source, &dest, 2).expect("copy"), 6);
        assert_eq!(read_text(&dest).expect("dest"), "abcXYZ");
    }
}
