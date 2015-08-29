#include "cab.h"
#include "handle.h"
#include <QDir>
#include <QDebug>

Cab::Cab(QString source, QString dest) :
  source(source),
  dest(dest),
  header(source) {
}

Cab::~Cab() {}

void Cab::run() {
  int cur = 0;
  int total = header.components.length();
  try {
    for (auto comp : header.components) {
      emit progress(cur * 100 / total); cur++;
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
                QString src = QString("%1/%2/%3")
                    .arg(source)
                    .arg(g.source)
                    .arg(header.files[i].name);
                src.replace(QRegExp("\\\\"), "/");
                src.replace(QRegExp("//+"), "/");
                if (!QFile::copy(src, filepath))
                  throw CabException(tr("Failed to copy %1 to %2")
                                     .arg(src)
                                     .arg(filepath));
              } else
                extractFile(i, filepath);
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
  //  qDebug() << filename;
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
      file.flags = handle.r8();
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
