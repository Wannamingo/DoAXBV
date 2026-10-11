package org.doaxbv.port;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** Read-only, bounded XDVDFS reader for original Xbox ISO and full disc dumps. */
final class XboxIso {
    private static final int SECTOR = 2048;
    private static final byte[] MAGIC = "MICROSOFT*XBOX*MEDIA".getBytes(StandardCharsets.US_ASCII);
    private static final long SCAN_LIMIT = 512L * 1024 * 1024;
    private final FileChannel channel;
    private final long length;
    private long base;

    static final class Entry {
        final long offset, size;
        Entry(long offset, long size) { this.offset = offset; this.size = size; }
    }
    private static final class Directory {
        final long sector, size;
        final String prefix;
        final int depth;
        Directory(long sector, long size, String prefix, int depth) {
            this.sector = sector; this.size = size; this.prefix = prefix; this.depth = depth;
        }
    }

    XboxIso(FileChannel channel) throws IOException {
        this.channel = channel;
        try {
            length = channel.size();
            channel.position(0);
        } catch (IOException error) {
            throw new IOException("ISO требует произвольного чтения. Скопируйте образ во внутреннюю память и выберите его снова.", error);
        }
    }

    private void range(long offset, long size) throws IOException {
        if (offset < 0 || size < 0 || offset > length || size > length - offset)
            throw new IOException("ISO обрезан или содержит неверные границы файлов");
    }

    private void read(long offset, byte[] bytes, int size) throws IOException {
        range(offset, size);
        ByteBuffer buffer = ByteBuffer.wrap(bytes, 0, size);
        while (buffer.hasRemaining()) {
            int n = channel.read(buffer, offset + buffer.position());
            if (n <= 0) throw new EOFException("Не удалось прочитать ISO полностью");
        }
    }

    private static boolean descriptor(byte[] bytes, int offset) {
        for (int i = 0; i < MAGIC.length; i++)
            if (bytes[offset + i] != MAGIC[i] || bytes[offset + SECTOR - MAGIC.length + i] != MAGIC[i]) return false;
        return true;
    }

    private byte[] volume() throws IOException {
        byte[] sector = new byte[SECTOR];
        // XISO omits the video area present before the game partition in full dumps.
        for (long candidate : new long[]{0, 0x18300000L, 0xFD90000L}) {
            long offset = candidate + 32L * SECTOR;
            if (offset + SECTOR > length) continue;
            read(offset, sector, SECTOR);
            if (descriptor(sector, 0)) { base = candidate; return sector; }
        }
        byte[] buffer = new byte[1024 * 1024];
        long end = Math.min(length, SCAN_LIMIT);
        for (long offset = 32L * SECTOR; offset + SECTOR <= end;) {
            int count = (int)(Math.min(buffer.length, end - offset) / SECTOR * SECTOR);
            read(offset, buffer, count);
            for (int i = 0; i < count; i += SECTOR) {
                if (descriptor(buffer, i)) {
                    base = offset + i - 32L * SECTOR;
                    return Arrays.copyOfRange(buffer, i, i + SECTOR);
                }
            }
            offset += count;
        }
        throw new IOException("Не найден раздел Xbox. Выберите ISO оригинальной USA-версии игры.");
    }

    private static int u16(byte[] bytes, int offset) {
        return (bytes[offset] & 255) | ((bytes[offset + 1] & 255) << 8);
    }
    private static long u32(byte[] bytes, int offset) {
        return (u16(bytes, offset) & 0xFFFFL) | ((long)u16(bytes, offset + 2) << 16);
    }

    Map<String, Entry> index() throws IOException {
        byte[] header = volume();
        ArrayDeque<Directory> directories = new ArrayDeque<>();
        directories.add(new Directory(u32(header, 20), u32(header, 24), "", 0));
        Set<Long> seenDirectories = new HashSet<>();
        Set<String> paths = new HashSet<>();
        Map<String, Entry> files = new LinkedHashMap<>();
        int entries = 0;
        long tableBytes = 0;
        while (!directories.isEmpty()) {
            Directory directory = directories.removeFirst();
            if (directory.depth > 32 || directory.size < 14 || directory.size > 4 * 1024 * 1024
                    || !seenDirectories.add(directory.sector) || (tableBytes += directory.size) > 16 * 1024 * 1024)
                throw new IOException("Повреждён каталог ISO");
            byte[] table = new byte[(int)directory.size];
            read(base + directory.sector * SECTOR, table, table.length);
            ArrayDeque<Integer> pending = new ArrayDeque<>();
            Set<Integer> visited = new HashSet<>();
            pending.add(0);
            while (!pending.isEmpty()) {
                int offset = pending.removeLast();
                if (offset < 0 || offset > table.length - 14 || !visited.add(offset) || ++entries > 20000)
                    throw new IOException("Повреждённое дерево файлов ISO");
                int nameLength = table[offset + 13] & 255;
                if (nameLength == 0 || nameLength > table.length - offset - 14)
                    throw new IOException("Повреждено имя файла ISO");
                for (int i = 0; i < nameLength; i++) {
                    int c = table[offset + 14 + i] & 255;
                    if (c < 32 || c > 126 || c == '/' || c == '\\' || c == ':')
                        throw new IOException("Недопустимый путь в ISO");
                }
                String name = new String(table, offset + 14, nameLength, StandardCharsets.US_ASCII);
                String path = directory.prefix + name;
                if (name.equals(".") || name.equals("..") || path.length() > 1024 || !paths.add(path.toLowerCase(Locale.ROOT)))
                    throw new IOException("Недопустимый или повторный путь в ISO");
                long sector = u32(table, offset + 4), size = u32(table, offset + 8);
                range(base + sector * SECTOR, size);
                if ((table[offset + 12] & 0x10) != 0) {
                    if (size != 0) directories.add(new Directory(sector, size, path + '/', directory.depth + 1));
                } else {
                    files.put(path, new Entry(base + sector * SECTOR, size));
                }
                int left = u16(table, offset), right = u16(table, offset + 2);
                if (left != 0) pending.add(left * 4);
                if (right != 0) pending.add(right * 4);
            }
        }
        return files;
    }

    InputStream open(Entry entry) throws IOException {
        range(entry.offset, entry.size);
        return new InputStream() {
            private long position;
            @Override public int read() throws IOException {
                byte[] one = new byte[1];
                return read(one, 0, 1) == -1 ? -1 : one[0] & 255;
            }
            @Override public int read(byte[] bytes, int offset, int count) throws IOException {
                if (offset < 0 || count < 0 || offset > bytes.length - count) throw new IndexOutOfBoundsException();
                if (count == 0) return 0;
                if (position == entry.size) return -1;
                int wanted = (int)Math.min(count, entry.size - position);
                int n = channel.read(ByteBuffer.wrap(bytes, offset, wanted), entry.offset + position);
                if (n <= 0) throw new EOFException("ISO обрезан при чтении игровых файлов");
                position += n;
                return n;
            }
        };
    }
}
