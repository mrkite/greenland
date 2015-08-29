#include "cab.h"
#include "handle.h"
#include <QDir>
#include <QCryptographicHash>
#include <QtEndian>
#include <QDebug>
#include <zlib.h>

Cab::Cab(QString source, QString dest) :
  source(source),
  dest(dest),
  header(source),
  cab(source) {
}

Cab::~Cab() {}

void Cab::run() {
  int cur = 0;
  int total = header.files.length();
  try {
    for (auto comp : header.components) {
      for (auto group: comp.groups) {
        if (header.groups.contains(group)) {
          Group &g = header.groups[group];
          for (int i = g.first; i <= g.last; i++) {
            if (i < header.files.length()) {
              QString filepath = QString("%1/%2/%3")
                  .arg(g.dest)
                  .arg(header.files[i].directory)
                  .arg(header.files[i].name);
              if (!filepath.contains("<TARGETDIR>"))
                continue;
              filepath.replace(QRegExp("<TARGETDIR>"), dest);
              filepath.replace(QRegExp("\\\\"), "/");
              filepath.replace(QRegExp("//+"), "/");

              QFileInfo fi(filepath);
              QDir dir;
              dir.mkpath(fi.absolutePath());

              if (header.files[i].offset == 0) {
                emit progress(QString("Copying %1").arg(header.files[i].name),
                              cur * 100 / total); cur++;
                QString src = QString("%1/%2/%3")
                    .arg(source)
                    .arg(g.source)
                    .arg(header.files[i].name);
                src.replace(QRegExp("\\\\"), "/");
                src.replace(QRegExp("//+"), "/");
                QFile::remove(filepath); //overwrite existing if necessary
                if (!QFile::copy(src, filepath))
                  throw CabException(tr("Failed to copy %1 to %2")
                                     .arg(src)
                                     .arg(filepath));
              } else {
                emit progress(QString("Extracting %1").arg(header.files[i].name),
                              cur * 100 / total); cur++;
                extractFile(i, filepath);
              }
            }
          }
        }
      }
    }
    emit finished();
  } catch (CabException e) {
    emit error(e.reason);
  }
}

void Cab::extractFile(int index, QString filename) {
  if (index >= header.files.length())
    throw CabException(tr("File #%1 doesn't exist").arg(index));

  auto &f = header.files[index];
  if (f.flags & 8) // invalid
    return;
  if (f.link & 1) { //previous
    extractFile(f.previous, filename);
    return;
  }

  QFile file(filename);
  file.open(QIODevice::WriteOnly);

  cab.seek(f.volume, index, f.offset, f.flags & 2);
  qint64 bytesLeft = f.size;
  if (f.flags & 4) // compressed
    bytesLeft = f.compressed;

  static const int CHUNK_SIZE = 64 * 1024;
  char buffer[CHUNK_SIZE];

  QCryptographicHash md5(QCryptographicHash::Algorithm::Md5);

  while (bytesLeft > 0) {
    if (f.flags & 4) { // compressed
      auto lenbytes = cab.read(2);
      bytesLeft -= 2;
      quint16 len = qFromLittleEndian<quint16>
          (reinterpret_cast<const uchar *>(lenbytes.constData()));
      auto raw = cab.read(len);
      bytesLeft -= len;

      z_stream stream;
      stream.next_in = (Bytef *)raw.constData();
      stream.avail_in = raw.length();
      stream.zalloc = Z_NULL;
      stream.zfree = Z_NULL;

      inflateInit2(&stream, -MAX_WBITS);
      do {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        inflate(&stream, Z_NO_FLUSH);
        md5.addData(buffer, CHUNK_SIZE - stream.avail_out);
        file.write(buffer, CHUNK_SIZE - stream.avail_out);
      } while (stream.avail_out == 0);
      inflateEnd(&stream);
    } else {
      auto raw = cab.read(bytesLeft);
      bytesLeft = 0;
      md5.addData(raw);
      file.write(raw);
    }
  }
  file.close();
  QByteArray res = md5.result();
  for (int i = 0; i < 0x10; i++) {
    if (res.at(i) != f.md5[i])
      throw CabException(QString("%1 md5 sum failed").arg(filename));
  }
}

Cab::Header::Header(QString path) {
  Handle handle(QString("%1/data1.hdr").arg(path));
  if (!handle.exists())
    throw CabException("Couldn't open data1.hdr");
  if (handle.r32() != 0x28635349)
    throw CabException("data1.hdr isn't valid");
  quint32 v = handle.r32();
  handle.skip(4); // volume info
  quint32 base = handle.r32();

  switch (v >> 24) {
  case 1:
    version = (v >> 12) & 0xf;
    break;
  default:
    version = (v & 0xffff) / 100;
    break;
  }

  handle.seek(base + 0xc);
  quint32 ftOffset = handle.r32();
  handle.skip(4); //unknown
  handle.skip(8); //ftsize and it's dup
  qint32 numDirs = handle.r32();
  handle.skip(8); //unknown
  qint32 numFiles = handle.r32();
  quint32 ftOffset2 = handle.r32();
  handle.skip(0xe); //unknown

  QList<quint32> groupOffsets;
  for (int i = 0; i < 71; i++)
    groupOffsets.append(handle.r32());
  QList<quint32> compOffsets;
  for (int i = 0; i < 71; i++)
    compOffsets.append(handle.r32());

  QList<quint32> fileTable;
  handle.seek(base + ftOffset);
  for (int i = 0; i < numDirs + numFiles; i++)
    fileTable.append(handle.r32());

  QList<QString> dirs;
  for (int i = 0; i < numDirs; i++) {
    QString dir;
    if (fileTable[i]) {
      handle.seek(base + ftOffset + fileTable[i]);
      if (version >= 17)
        dir = QString::fromUtf16(handle.rs16());
      else
        dir = QString::fromUtf8(handle.rs());
    }
    dirs.append(dir);
  }

  for (quint32 off : compOffsets) {
    while (off) {
      handle.seek(base + off + 4); //skip name
      quint32 desc = handle.r32();
      off = handle.r32(); //linked list

      handle.seek(base + desc);
      quint32 name = handle.r32();
      handle.skip(0x6b);
      if (version == 0 || version == 5)
        handle.skip(1);
      quint16 numGroups = handle.r16();
      quint32 gOff = handle.r32();
      Component comp;
      handle.seek(base + name);
      if (version >= 17)
        comp.name = QString::fromUtf16(handle.rs16());
      else
        comp.name = QString::fromUtf8(handle.rs());
      for (int i = 0; i < numGroups; i++) {
        handle.seek(base + gOff + i * 4);
        handle.seek(base + handle.r32());
        if (version >= 17)
          comp.groups.append(QString::fromUtf16(handle.rs16()));
        else
          comp.groups.append(QString::fromUtf8(handle.rs()));
      }
      components.append(comp);
    }
  }

  for (quint32 off : groupOffsets) {
    while (off) {
      handle.seek(base + off + 4); //skip name
      quint32 desc = handle.r32();
      off = handle.r32(); //linked list

      handle.seek(base + desc);
      quint32 name = handle.r32();
      handle.skip(0x12);
      if (version == 0 || version == 5)
        handle.skip(0x36);
      Group group;
      group.first = handle.r32();
      group.last = handle.r32();

      quint32 sourcefolder = handle.r32();
      handle.skip(0x18); //unused strings
      quint32 destfolder = handle.r32();

      handle.seek(base + name);
      if (version >= 17)
        group.name = QString::fromUtf16(handle.rs16());
      else
        group.name = QString::fromUtf8(handle.rs());
      handle.seek(base + sourcefolder);
      if (version >= 17)
        group.source = QString::fromUtf16(handle.rs16());
      else
        group.source = QString::fromUtf8(handle.rs());
      handle.seek(base + destfolder);
      if (version >= 17)
        group.dest = QString::fromUtf16(handle.rs16());
      else
        group.dest = QString::fromUtf8(handle.rs());
      groups.insert(group.name, group);
    }
  }

  for (int i = 0; i < numFiles; i++) {
    File file;
    file.name = "";
    file.directory = "";
    quint32 name, dir;
    if (version == 0 || version == 5) {
      handle.seek(base + ftOffset + fileTable[i + numDirs]);
      name = handle.r32();
      dir = handle.r32();
      file.flags = handle.r16();
      file.size = handle.r32();
      file.compressed = handle.r32();
      handle.skip(0x14);
      file.offset = handle.r32();
      for (int m = 0; m < 0x10; m++)
        file.md5[m] = handle.r8();
      file.link = 0;
      file.volume = 1;
    } else {
      handle.seek(base + ftOffset + ftOffset2 + i * 0x57);
      file.flags = handle.r16();
      file.size = handle.r64();
      file.compressed = handle.r64();
      file.offset = handle.r64();
      for (int m = 0; m < 0x10; m++)
        file.md5[m] = handle.r8();
      handle.skip(0x10);
      name = handle.r32();
      dir = handle.r16();
      handle.skip(0xc);
      file.previous = handle.r32();
      file.next = handle.r32();
      file.link = handle.r8();
      file.volume = handle.r16();
    }
    if (name) {
      handle.seek(base + ftOffset + name);
      if (version >= 17)
        file.name = QString::fromUtf16(handle.rs16());
      else
        file.name = QString::fromUtf8(handle.rs());
    }
    if (dir)
      file.directory = dirs[dir];
    files.append(file);
  }
}

Cabinet::Cabinet(QString root) : base(root) {
  curVolume = 0;
}

void Cabinet::seek(int volume, int index, int offset, bool obfuscated) {
  curIndex = index;
  unobfuscate = obfuscated;
  if (curVolume != volume) {
    curVolume = volume;
    open();
  }
  if (index == lastIndex) {
    end = lastCompressed;
    handle->seek(lastOffset);
  } else {
    end = handle->length() - offset;
    handle->seek(offset);
  }
  obOffs = 0;
}

void Cabinet::open() {
  do {
    QString path = QString("%1/data%2.cab").arg(base).arg(curVolume);
    handle = QSharedPointer<Handle>(new Handle(path));
    if (!handle->exists())
      throw CabException(QString("Couldn't open %1").arg(path));
    if (handle->r32() != 0x28635349)
      throw CabException(QString("data%1.cab isn't valid").arg(curVolume));

    quint32 v = handle->r32();
    handle->skip(0xc); //volume info, base, size

    quint32 version = 0;
    switch (v >> 24) {
    case 1:
      version = (v >> 12) & 0xf;
      break;
    default:
      version = (v & 0xffff) / 100;
      break;
    }

    switch (version) {
    case 0:
    case 5:
      handle->skip(8); //offset
      firstIndex = handle->r32();
      lastIndex = handle->r32();
      firstOffset = handle->r32();
      firstSize = handle->r32();
      firstCompressed = handle->r32();
      lastOffset = handle->r32();
      lastSize = handle->r32();
      lastCompressed = handle->r32();
      break;
    default:
      handle->skip(8); //offset
      firstIndex = handle->r32();
      lastIndex = handle->r32();
      firstOffset = handle->r64();
      firstSize = handle->r64();
      firstCompressed = handle->r64();
      lastOffset = handle->r64();
      lastSize = handle->r64();
      lastCompressed = handle->r64();
      break;
    }
    if (version == 5 && curIndex > lastIndex)
      curVolume++;
    else
      break;
  } while (true);
}

QByteArray Cabinet::read(int len) {
  QByteArray result;

  while (len > 0) {
    int toread = len;
    if (toread > end)
      toread = end;

    result.append(handle->read(toread), toread);
    len -= toread;
    end -= toread;
    if (len) {
      curVolume++;
      open();
      if (curIndex == firstIndex)
        handle->seek(firstOffset);
      else if (curIndex == lastIndex)
        handle->seek(lastOffset);
    }
  }
  if (unobfuscate) {
    for (int i = 0; i < result.length(); i++, obOffs++) {
      quint8 ch = result[i] ^ 0xd5;
      result[i] = ((ch >> 2) | (ch << 6)) - (obOffs % 0x47);
    }
  }
  return result;
}
