#include "mve.h"
#include <QDebug>

MVE::MVE(QSharedPointer<Handle> handle, QWidget *parent)
  : QScrollArea(parent), handle(handle) {
  surface = NULL;
  surface2 = NULL;

  view = new QLabel(this);
  setWidget(view);

  connect(&timer, SIGNAL(timeout()),
          this, SLOT(readChunk()));

  memset(palette, 0, 256 * 3);
  handle->seek(0x1a);
  updateScreen = false;
  readChunk();
}

MVE::~MVE() {
  if (surface != NULL)
    delete[] surface;
  if (surface2 != NULL)
    delete[] surface2;
}

void MVE::readChunk() {
  quint32 chunkSize = 1;
  while (chunkSize & 0xffff) {
    chunkSize = handle->r32();
    qint64 ofs = handle->tell();
    if (!(chunkSize & 0xffff))
      break;
    handleChunk(chunkSize);
    handle->seek(ofs + (chunkSize & 0xffff));
    if (updateScreen)
      break;
  }
  if ((chunkSize & 0xffff) == 0)
    timer.stop();
}


void MVE::handleChunk(quint32 size) {
  quint32 offset = 0;

  quint32 da, delaydiv, delay;
  quint16 wa, wb, wc, wd;

  quint8 *control;

  do {
    qint64 offs = handle->tell();
    quint16 packetSize = handle->r16();
    quint8 opcode = handle->r8();
    quint8 param = handle->r8();
    switch (opcode) {
    case 0: return;
    case 1: return;
    case 2: //timing
      da = handle->r32();
      wb = handle->r16();
      delaydiv = 1000 / wb;
      delay = da / delaydiv;
      timer.start(delay);
      break;
    case 3: //init sound
      break;
    case 4: //play sound
      break;
    case 5: //init video
      framewidth = handle->r16() * 8;
      frameheight = handle->r16() * 8;
      initVideo();
      break;
    case 7: //show frame
      wa = handle->r16();
      wb = handle->r16();
      wc = 0;
      if (param > 0) wc = handle->r16();
      saveFrame(wc);
      updateScreen = true;
      break;
    case 8: //sound data
      break;
    case 9: //sound control
      break;
    case 10: //init movie
      width = handle->r16();
      height = handle->r16();
      break;
    case 12: //set palette
      wa = handle->r16();
      wb = handle->r16();
      for (int i = 0; i < wb * 3; i++)
        palette[wa * 3 + i] = handle->r8();
      break;
    case 15: //decoder control
      control = (quint8 *)handle->read(packetSize);
      break;
    case 17: //decode blocks
      handle->skip(4); //unknown
      wa = handle->r16();
      wb = handle->r16();
      wc = handle->r16();
      wd = handle->r16();
      if (handle->r16() & 1) {
        quint8 *temp = surface2;
        surface2 = surface;
        surface = temp;
      }
      decodeBlocks(control, wa, wb, wc, wd);
      break;
    case 19: //unknown
    case 21: // unknown
      break;
    default:
      qDebug() << "unknown opcode: " << (int)opcode;
      break;
    }
    handle->seek(offs + packetSize);
    offset += packetSize;
  } while (offset < size);
}

void MVE::initVideo() {
  surface = new quint8[framewidth * frameheight];
  surface2 = new quint8[framewidth * frameheight];
  image = new QImage(framewidth, frameheight, QImage::Format_RGB888);
}

void MVE::saveFrame(quint16) {
  quint8 *source = surface + (realY * framewidth) + realX;
  uchar *bits = image->bits();
  int stride = image->bytesPerLine();
  for (uint y = 0; y < realH; y++) {
    int out = (realY + y) * stride + realX;
    quint8 *src = source;
    for (uint x = 0; x < realW; x++) {
      quint8 c = *src++;
      bits[out++] = palette[c * 3] * 4;
      bits[out++] = palette[c * 3 + 1] * 4;
      bits[out++] = palette[c * 3 + 2] * 4;
    }
    source += framewidth;
  }
  view->setPixmap(QPixmap::fromImage(*image));
}

void MVE::decodeBlocks(quint8 *control, quint16 rx, quint16 ry,
                       quint16 w, quint16 h) {
  realX = rx * 8;
  realY = ry * 8;
  realW = w * 8;
  realH = h * 8;
  quint8 *dest = surface;
  if (rx || ry)
    dest += (realY * framewidth) + realX;
  quint8 *destLine = dest;
  quint8 key;

  for (int y = 0; y < h; y++) {
    dest = destLine;
    for (int x = 0; x < w; x++) {
      if (!(x & 1)) key = *control++;
      else key >>= 4;

      switch (key & 0xf) {
      case 0: sameBlock(dest); break;
      case 1: break; // skip this block
      case 2: copyPreBlock(dest); break;
      case 3: copyBlock(dest); break;
      case 4: copyBackBlock(dest); break;
      case 5: shiftBackBlock(dest); break;
      case 6: qDebug() << "FAIL"; break;
      case 7: draw2ColBitBlock(dest); break;
      case 8: draw8ColBitBlock(dest); break;
      case 9: draw4ColBitBlock(dest); break;
      case 10: draw16ColBitBlock(dest); break;
      case 11: drawFullBlock(dest); break;
      case 12: fill2x2CheckedBlock(dest); break;
      case 13: fill4x4CheckedBlock(dest); break;
      case 14: fillSolidBlock(dest); break;
      case 15: fill1x1CheckedBlock(dest); break;
      }
      dest += 8;
    }
    destLine += framewidth * 8;
  }
}

void MVE::sameBlock(quint8 *dest) {
  quint32 delta = dest - surface;
  quint8 *ptr = surface2 + delta;
  for (int y = 0; y < 8; y++) {
    memcpy(dest, ptr, 8);
    dest += framewidth;
    ptr += framewidth;
  }
}

void MVE::copyPreBlock(quint8 *dest) {
  quint8 val = handle->r8();
  char y, x;

  if (val >= 56) {
    val -= 56;
    y = (val / 29) + 8;
    x = (val % 29) - 14;
  } else {
    y = val / 7;
    x = (val % 7) + 8;
  }
  quint8 *ptr = dest + y * framewidth + x;
  for (int y = 0; y < 8; y++) {
    memcpy(dest, ptr, 8);
    dest += framewidth;
    ptr += framewidth;
  }
}

void MVE::copyBlock(quint8 *dest) {
  quint8 val = handle->r8();
  char y, x;

  if (val >= 56) {
    val -= 56;
    y = -((val / 29) + 8);
    x = -((val % 29) - 14);
  } else {
    y = -(val / 7);
    x = -((val % 7) + 8);
  }
  quint8 *ptr = dest + y * framewidth + x;
  for (int y = 0; y < 8; y++) {
    memcpy(dest, ptr, 8);
    dest += framewidth;
    ptr += framewidth;
  }
}

void MVE::copyBackBlock(quint8 *dest) {
  quint8 val = handle->r8();
  char x = (val & 15) - 8;
  char y = (val >> 4) - 8;
  quint32 delta = dest - surface;
  quint8 *ptr = surface2 + delta + y * framewidth + x;
  for (int y = 0; y < 8; y++) {
    memcpy(dest, ptr, 8);
    dest += framewidth;
    ptr += framewidth;
  }
}

void MVE::shiftBackBlock(quint8 *dest) {
  char x = handle->r8();
  char y = handle->r8();
  quint32 delta = dest - surface;
  quint8 *ptr = surface2 + delta + y * framewidth + x;
  for (int y = 0; y < 8; y++) {
    memcpy(dest, ptr, 8);
    dest += framewidth;
    ptr += framewidth;
  }
}

void MVE::draw2ColBitBlock(quint8 *dest) {
  quint8 lo = handle->r8();
  quint8 hi = handle->r8();

  if (lo < hi) {
    for (int y = 0; y < 8; y++) {
      quint8 val = handle->r8();
      for (int x = 0; x < 8; x++) {
        dest[x] = (val & 1) ? hi : lo;
        val >>= 1;
      }
      dest += framewidth;
    }
  } else {
    quint8 val;
    for (int y = 0; y < 4; y++) {
      if (!(y & 1)) val = handle->r8();
      for (int x = 0; x < 4; x++) {
        dest[x * 2] = (val & 1) ? hi : lo;
        dest[x * 2 + 1] = dest[x * 2];
        dest[x * 2 + framewidth] = dest[x * 2];
        dest[x * 2 + 1 + framewidth] = dest[x * 2];
        val >>= 1;
      }
      dest += framewidth * 2;
    }
  }
}

void MVE::draw8ColBitBlock(quint8 *dest) {

  quint8 *source = (quint8 *)handle->read(12);
  quint8 lo = source[0];
  quint8 hi = source[1];
  quint8 lo2 = source[6];
  quint8 hi2 = source[7];

  quint8 key, key2;

  if (lo < hi) {
    for (int i = 0; i < 2; i++) {
      lo = source[i * 4];
      hi = source[i * 4 + 1];
      lo2 = source[i * 4 + 8];
      hi2 = source[i * 4 + 9];
      for (int y = 0; y < 4; y++) {
        if (!(y & 1)) key = source[i * 4 + y + 2];
        for (int x = 0; x < 4; x++) {
          dest[x] = (key & 1) ? hi : lo;
          key >>= 1;
        }
        if (!(y & 1)) key2 = source[i * 4 + y + 10];
        for (int x = 0; x < 4; x++) {
          dest[x + 4] = (key2 & 1) ? hi2 : lo2;
          key2 >>= 1;
        }
        dest += framewidth;
      }
    }
    handle->skip(4);
  } else if (lo2 < hi2) {
    for (int y = 0; y < 8; y++) {
      if (!(y & 1)) key = source[y + 2];
      for (int x = 0; x < 4; x++) {
        dest[x] = (key & 1) ? hi : lo;
        key >>= 1;
      }
      if (!(y & 1)) key2 = source[y + 8];
      for (int x = 0; x < 4; x++) {
        dest[x + 4] = (key2 & 1) ? hi2 : lo2;
        key2 >>= 1;
      }
      dest += framewidth;
    }
  } else {
    for (int i = 0; i < 2; i++) {
      lo = source[i * 6];
      hi = source[i * 6 + 1];
      for (int y = 0; y < 4; y++) {
        key = source[i * 6 + y + 2];
        for (int x = 0; x < 8; x++) {
          dest[x] = (key & 1) ? hi : lo;
          key >>= 1;
        }
        dest += framewidth;
      }
    }
  }
}

void MVE::draw4ColBitBlock(quint8 *dest) {
  quint8 key, val;
  quint8 *cmd = (quint8 *)handle->read(4);
  if (cmd[0] < cmd[1] && cmd[2] < cmd[3]) {
    for (int y = 0; y < 16; y++) {
      key = cmd[4 + y];
      for (int x = 0; x < 4; x++) {
        val = cmd[key & 3];
        key >>= 2;
        dest[x + (y & 1) * 4] = val;
      }
      if (y & 1)
        dest += framewidth;
    }
    handle->skip(16);
  } else if (cmd[0] < cmd[1]) {
    for (int y = 0; y < 4; y++) {
      key = cmd[4 + y];
      for (int x = 0; x < 8; x += 2) {
        val = cmd[key & 3];
        key >>= 2;
        dest[x] = val;
        dest[x + 1] = val;
        dest[x + framewidth] = val;
        dest[x + 1 + framewidth] = val;
      }
      dest += framewidth * 2;
    }
    handle->skip(4);
  } else if (cmd[3] < cmd[4]) {
    for (int y = 0; y < 8; y++) {
      key = cmd[4 + y];
      for (int x = 0; x < 8; x += 2) {
        val = cmd[key & 3];
        key >>= 2;
        dest[x] = val;
        dest[x + 1] = val;
      }
      dest += framewidth;
    }
    handle->skip(8);
  } else {
    for (int y = 0; y < 8; y++) {
      key = cmd[4 + y];
      for (int x = 0; x < 4; x++) {
        val = cmd[key & 3];
        key >>= 2;
        dest[x + (y & 1) * 4] = val;
        dest[x + framewidth + (y & 1) * 4] = val;
      }
      if (y & 1)
        dest += framewidth * 2;
    }
    handle->skip(8);
  }
}

void MVE::draw16ColBitBlock(quint8 *dest) {
  quint8 cmd[4], cmd2[4], key, val;

  quint8 *source = (quint8 *)handle->read(24);

  quint8 lo = source[0];
  quint8 hi = source[1];
  quint8 lo2 = source[12];
  quint8 hi2 = source[13];

  if (lo < hi) {
    for (int i = 0; i < 2; i++) {
      for (int y = 0; y < 4; y++) {
        cmd[y] = source[i * 8 + y];
        cmd2[y] = source[i * 8 + 16 + y];
      }
      for (int y = 0; y < 4; y++) {
        key = source[i * 8 + y + 4];
        for (int x = 0; x < 4; x++) {
          val = cmd[key & 3];
          dest[x] = val;
          key >>= 2;
        }
        key = source[i * 8 + y + 20];
        for (int x = 0; x < 4; x++) {
          val = cmd2[key & 3];
          dest[x + 4] = val;
          key >>= 2;
        }
        dest += framewidth;
      }
    }
    handle->skip(8);
  } else if (lo2 < hi2) {
    for (int y = 0; y < 4; y++) {
      cmd[y] = source[y];
      cmd2[y] = source[y + 12];
    }
    for (int y = 0; y < 8; y++) {
      key = source[y + 4];
      for (int x = 0; x < 4; x++) {
        val = cmd[key & 3];
        dest[x] = val;
        key >>= 2;
      }
      key = source[y + 16];
      for (int x = 0; x < 4; x++) {
        val = cmd2[key & 3];
        dest[x + 4] = val;
        key >>= 2;
      }
      dest += framewidth;
    }
  } else {
    for (int i = 0; i < 2; i++) {
      for (int y = 0; y < 4; y++)
        cmd[y] = source[i * 12 + y];
      for (int y = 0; y < 8; y++) {
        key = source[i * 12 + y + 4];
        for (int x = 0; x < 4; x++) {
          val = cmd[key & 3];
          dest[x + (y & 1) * 4] = val;
          key >>= 2;
        }
        if (y & 1)
          dest += framewidth;
      }
    }
  }
}

void MVE::drawFullBlock(quint8 *dest) {
  quint8 *source = (quint8 *)handle->read(64);
  for (int y = 0; y < 8; y++) {
    memcpy(dest, source + y * 8, 8);
    dest += framewidth;
  }
}

void MVE::fill2x2CheckedBlock(quint8 *dest) {
  quint8 lo, hi;
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 2; x++) {
      lo = handle->r8();
      hi = handle->r8();
      memset(dest + x * 4, lo, 2);
      memset(dest + x * 4 + 2, hi, 2);
      memset(dest + framewidth + x * 4, lo, 2);
      memset(dest + framewidth + x * 4 + 2, hi, 2);
    }
    dest += framewidth * 2;
  }
}

void MVE::fill4x4CheckedBlock(quint8 *dest) {
  quint8 hi, lo;

  for (int y = 0; y < 2; y++) {
    lo = handle->r8();
    hi = handle->r8();
    for (int x = 0; x < 2; x++) {
      memset(dest, lo, 4);
      memset(dest + 4, hi, 4);
      memset(dest + framewidth, lo, 4);
      memset(dest + framewidth + 4, hi, 4);
      dest += framewidth * 2;
    }
  }
}

void MVE::fillSolidBlock(quint8 *dest) {
  quint8 val = handle->r8();
  for (int y = 0; y < 8; y++) {
    memset(dest, val, 8);
    dest += framewidth;
  }
}

void MVE::fill1x1CheckedBlock(quint8 *dest) {
  quint8 lo = handle->r8();
  quint8 hi = handle->r8();
  quint8 tmp;

  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x += 2) {
      dest[x] = hi;
      dest[x + 1] = lo;
    }
    tmp = hi;
    hi = lo;
    lo = hi;
    dest += framewidth;
  }
}
