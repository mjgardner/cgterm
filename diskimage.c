#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diskimage.h"


/* allocate rawname and convert */
unsigned char *di_name_to_rawname(char *name) {
  unsigned char *rawname;
  int i;

  if ((rawname = malloc(16)) == NULL) {
    return(NULL);
  }
  memset(rawname, 0xa0, 16);
  for (i = 0; name[i]; ++i) {
    rawname[i] = name[i];
  }
  return(rawname);
}


/* return write interleave */
int interleave(ImageType type) {
  switch (type) {
  case D64:
    return(10);
    break;
  case D71:
    return(6);
    break;
  default:
    return(1);
    break;
  }
}


/* return number of tracks for image type */
int di_tracks(ImageType type) {
  switch (type) {
  case D64:
    return(35);
    break;
  case D71:
    return(70);
    break;
  case D81:
    return(80);
    break;
  }
  return(0);
}


/* return disk geometry for track */
int di_sectors_per_track(ImageType type, int track) {
  switch (type) {
  case D71:
    if (track > 35) {
      track -= 35;
    }
    // fall through
  case D64:
    if (track < 18) {
      return(21);
    } else if (track < 25) {
      return(19);
    } else if (track < 31) {
      return(18);
    } else {
      return(17);
    }
    break;
  case D81:
    return(40);
    break;
  }
  return(0);
}

/* convert track, sector to blocknum */
int get_block_num(ImageType type, TrackSector ts) {
  int block;

  switch (type) {
  case D64:
    if (ts.track < 18) {
      block = (ts.track - 1) * 21;
    } else if (ts.track < 25) {
      block = (ts.track - 18) * 19 + 17 * 21;
    } else if (ts.track < 31) {
      block = (ts.track - 25) * 18 + 17 * 21 + 7 * 19;
    } else {
      block = (ts.track - 31) * 17 + 17 * 21 + 7 * 19 + 6 * 18;
    }
    return(block + ts.sector);
    break;
  case D71:
    if (ts.track > 35) {
      block = 683;
      ts.track -= 35;
    } else {
      block = 0;
    }
    if (ts.track < 18) {
      block += (ts.track - 1) * 21;
    } else if (ts.track < 25) {
      block += (ts.track - 18) * 19 + 17 * 21;
    } else if (ts.track < 31) {
      block += (ts.track - 25) * 18 + 17 * 21 + 7 * 19;
    } else {
      block += (ts.track - 31) * 17 + 17 * 21 + 7 * 19 + 6 * 18;
    }
    return(block + ts.sector);
    break;
  case D81:
    return((ts.track - 1) * 40 + ts.sector);
    break;
  }
  return(0);
}


/* get a pointer to block data */
unsigned char *get_ts_addr(DiskImage *di, TrackSector ts) {
  return(di->image + get_block_num(di->type, ts) * 256);
}


/* return a pointer to the next block in the chain */
TrackSector next_ts_in_chain(DiskImage *di, TrackSector ts) {
  unsigned char *p;
  TrackSector newts;

  p = get_ts_addr(di, ts);
  newts.track = p[0];
  newts.sector = p[1];
  return(newts);
}


/* return number of free blocks in track */
int di_track_blocks_free(DiskImage *di, int track) {
  unsigned char *bam;

  switch (di->type) {
  case D64:
    bam = get_ts_addr(di, di->bam);
    break;
  case D71:
    if (track <= 35) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      track -= 35;
    }
    break;
  case D81:
    if (track <= 40) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      track -= 40;
    }
    return(bam[track * 6 + 10]);
    break;
  }
  return(bam[track * 4]);
}


/* count number of free blocks */
int blocks_free(DiskImage *di) {
  unsigned char *bam;
  int i, blocks;

  blocks = 0;
  switch (di->type) {
  case D64:
    bam = get_ts_addr(di, di->bam);
    for (i = 1; i <= 35; ++i) {
      if (i != 18) {
	blocks += bam[i * 4];
      }
    }
    break;
  case D71:
    bam = get_ts_addr(di, di->bam);
    for (i = 1; i <= 35; ++i) {
      if (i != 18) {
	blocks += bam[i * 4];
      }
    }
    bam = get_ts_addr(di, di->bam2);
    for (i = 1; i <= 35; ++i) {
      if (i != 18) {
	blocks += bam[i * 4];
      }
    }
    break;
  case D81:
    bam = get_ts_addr(di, di->bam);
    for (i = 1; i <= 39; ++i) {
      blocks += bam[i * 6 + 10];
    }
    bam = get_ts_addr(di, di->bam2);
    for (i = 1; i <= 40; ++i) {
      blocks += bam[i * 6 + 10];
    }
    break;
  }
  return(blocks);
}


/* check if track, sector is free in BAM */
int is_ts_free(DiskImage *di, TrackSector ts) {
  unsigned char mask;
  unsigned char *bam;

  switch (di->type) {
  case D64:
    bam = get_ts_addr(di, di->bam);
    if (bam[ts.track * 4]) {
      mask = 1<<(ts.sector & 7);
      return(bam[ts.track * 4 + ts.sector / 8 + 1] & mask ? 1 : 0);
    } else {
      return(0);
    }
    break;
  case D71:
    mask = 1<<(ts.sector & 7);
    if (ts.track < 36) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      ts.track -= 35;
    }
    return(bam[ts.track * 4 + ts.sector / 8 + 1] & mask ? 1 : 0);
    break;
  case D81:
    mask = 1<<(ts.sector & 7);
    if (ts.track < 41) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      ts.track -= 40;
    }
    return(bam[ts.track * 6 + ts.sector / 8 + 11] & mask ? 1 : 0);
    break;
  }
  return(0);
}


/* allocate track, sector in BAM */
void alloc_ts(DiskImage *di, TrackSector ts) {
  unsigned char mask;
  unsigned char *bam;

  di->modified = 1;
  switch (di->type) {
  case D64:
    bam = get_ts_addr(di, di->bam);
    bam[ts.track * 4] -= 1;
    mask = 1<<(ts.sector & 7);
    bam[ts.track * 4 + ts.sector / 8 + 1] &= ~mask;
    break;
  case D71:
    if (ts.track < 36) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      ts.track -= 35;
    }
    bam[ts.track * 4] -= 1;
    mask = 1<<(ts.sector & 7);
    bam[ts.track * 4 + ts.sector / 8 + 1] &= ~mask;
    break;
  case D81:
    if (ts.track < 41) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      ts.track -= 40;
    }
    bam[ts.track * 6 + 10] -= 1;
    mask = 1<<(ts.sector & 7);
    bam[ts.track * 6 + ts.sector / 8 + 11] &= ~mask;
    break;
  }
}


/* allocate next available block */
TrackSector alloc_next_ts(DiskImage *di, TrackSector prevts) {
  unsigned char *bam;
  int spt, s1, s2, t1, t2, bpt, boff;
  TrackSector ts;

  switch (di->type) {
  default:
  case D64:
    s1 = 1;
    t1 = 35;
    s2 = 1;
    t2 = 35;
    bpt = 4;
    boff = 0;
    break;
  case D71:
    s1 = 1;
    t1 = 35;
    s2 = 36;
    t2 = 70;
    bpt = 4;
    boff = 0;
    break;
  case D81:
    s1 = 1;
    t1 = 40;
    s2 = 41;
    t2 = 80;
    bpt = 6;
    boff = 10;
    break;
  }

  bam = get_ts_addr(di, di->bam);
  for (ts.track = s1; ts.track <= t1; ++ts.track) {
    if  (bam[ts.track * bpt + boff]) {
      spt = di_sectors_per_track(di->type, ts.track);
      ts.sector = (prevts.sector + interleave(di->type)) % spt;
      for (; ; ts.sector = (ts.sector + 1) % spt) {
	if (is_ts_free(di, ts)) {
	  alloc_ts(di, ts);
	  return(ts);
	}
      }
    }
  }

  if (di->type == D71 || di->type == D81) {
    bam = get_ts_addr(di, di->bam2);
    for (ts.track = s2; ts.track <= t2; ++ts.track) {
      if  (bam[(ts.track - t1) * bpt + boff]) {
	spt = di_sectors_per_track(di->type, ts.track);
	ts.sector = (prevts.sector + interleave(di->type)) % spt;
	for (; ; ts.sector = (ts.sector + 1) % spt) {
	  if (is_ts_free(di, ts)) {
	    alloc_ts(di, ts);
	    return(ts);
	  }
	}
      }
    }
  }

  ts.track = 0;
  ts.sector = 0;
  return(ts);
}


/* allocate next available directory block */
TrackSector alloc_next_dir_ts(DiskImage *di) {
  unsigned char *p;
  int spt;
  TrackSector ts, lastts;

  if (di_track_blocks_free(di, di->bam.track)) {
    ts.track = di->bam.track;
    ts.sector = 0;
    while (ts.track) {
      lastts = ts;
      ts = next_ts_in_chain(di, ts);
    }
    ts.track = lastts.track;
    ts.sector = lastts.sector + 3;
    spt = di_sectors_per_track(di->type, ts.track);
    for (; ; ts.sector = (ts.sector + 1) % spt) {
      if (is_ts_free(di, ts)) {
	alloc_ts(di, ts);
	p = get_ts_addr(di, lastts);
	p[0] = ts.track;
	p[1] = ts.sector;
	p = get_ts_addr(di, ts);
	memset(p, 0, 256);
	p[1] = 0xff;
	return(ts);
      }
    }
  } else {
    ts.track = 0;
    ts.sector = 0;
    return(ts);
  }
}


/* free a block in the BAM */
void free_ts(DiskImage *di, TrackSector ts) {
  unsigned char mask;
  unsigned char *bam;

  switch (di->type) {
  case D64:
    mask = 1<<(ts.sector & 7);
    bam = get_ts_addr(di, di->bam);
    bam[ts.track * 4 + ts.sector / 8 + 1] |= mask;
    bam[ts.track * 4] += 1;
    break;
  case D71:
    if (ts.track < 36) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      ts.track -= 35;
    }
    mask = 1<<(ts.sector & 7);
    bam[ts.track * 4 + ts.sector / 8 + 1] |= mask;
    bam[ts.track * 4] += 1;
    break;
  case D81:
    if (ts.track < 41) {
      bam = get_ts_addr(di, di->bam);
    } else {
      bam = get_ts_addr(di, di->bam2);
      ts.track -= 40;
    }
    mask = 1<<(ts.sector & 7);
    bam[ts.track * 6 + ts.sector / 8 + 11] |= mask;
    bam[ts.track * 6 + 10] += 1;
    break;
  default:
    break;
  }
}


/* free a chain of blocks */
void free_chain(DiskImage *di, TrackSector ts) {
  while (ts.track) {
    free_ts(di, ts);
    ts = next_ts_in_chain(di, ts);
  }
}


DiskImage *di_load_image(char *name) {
  FILE *file;
  int filesize, l, read;
  DiskImage *di;

  /* open image */
  if ((file = fopen(name, "rb")) == NULL) {
    puts("fopen failed");
    return(NULL);
  }

  /* get file size*/
  if (fseek(file, 0, SEEK_END)) {
    puts("fseek failed");
    fclose(file);
    return(NULL);
  }
  filesize = ftell(file);
  fseek(file, 0, SEEK_SET);

  if ((di = malloc(sizeof(*di))) == NULL) {
    puts("malloc failed");
    fclose(file);
    return(NULL);
  }

  /* check image type */
  switch (filesize) {
  case 174848: // standard D64
    di->type = D64;
    di->bam.track = 18;
    di->bam.sector = 0;
    break;
  case 349696:
    di->type = D71;
    di->bam.track = 18;
    di->bam.sector = 0;
    di->bam2.track = 53;
    di->bam2.sector = 0;
    break;
  case 819200:
    di->type = D81;
    di->bam.track = 40;
    di->bam.sector = 1;
    di->bam2.track = 41;
    di->bam2.sector = 2;
    break;
  default:
    puts("unknown type");
    free(di);
    fclose(file);
    return(NULL);
  }

  di->size = filesize;

  /* allocate buffer for image */
  if ((di->image = malloc(filesize)) == NULL) {
    puts("image malloc failed");
    free(di);
    fclose(file);
    return(NULL);
  }

  /* read file into buffer */
  read = 0;
  while (read < filesize) {
    if ((l = fread(di->image, 1, filesize - read, file))) {
      read += l;
    } else {
      puts("fread failed");
      free(di->image);
      free(di);
      fclose(file);
      return(NULL);
    }
  }

  di->filename = malloc(strlen(name) + 1);
  strcpy(di->filename, name);
  di->openfiles = 0;
  di->blocksfree = blocks_free(di);
  di->modified = 0;
  return(di);
}


void di_sync(DiskImage *di) {
  FILE *file;
  int l, left;
  unsigned char *image;

  if ((file = fopen(di->filename, "wb"))) {
    image = di->image;
    left = di->size;
    l = 0;
    while (left) {
      if ((l = fwrite(image, 1, left, file)) == 0) {
	fclose(file);
	return;
      }
      left -= l;
      image += l;
    }
    fclose(file);
    di->modified = 0;
  }
}


void di_free_image(DiskImage *di) {
  if (di->modified) {
    di_sync(di);
  }
  if (di->filename) {
    free(di->filename);
  }
  free(di->image);
  free(di);
}


RawDirEntry *find_file_entry(DiskImage *di, unsigned char *rawname, FileType type) {
  unsigned char *buffer;
  TrackSector ts;
  RawDirEntry *rde;
  int offset;

  ts = next_ts_in_chain(di, di->bam);
  while (ts.track) {
    buffer = get_ts_addr(di, ts);
    for (offset = 0; offset < 256; offset += 32) {
      rde = (RawDirEntry *)(buffer + offset);
      if ((rde->type & ~0x40) == (type | 0x80)) {
	if (strncmp(rawname, rde->rawname, 16) == 0) {
	  return(rde);
	}
      }
    }
    ts = next_ts_in_chain(di, ts);
  }
  return(NULL);
}


RawDirEntry *alloc_file_entry(DiskImage *di, unsigned char *rawname, FileType type) {
  unsigned char *buffer;
  TrackSector ts, lastts;
  RawDirEntry *rde;
  int offset;

  /* check if file already exists */
  ts = next_ts_in_chain(di, di->bam);
  while (ts.track) {
    buffer = get_ts_addr(di, ts);
    for (offset = 0; offset < 256; offset += 32) {
      rde = (RawDirEntry *)(buffer + offset);
      if (rde->type) {
	if (strncmp(rawname, rde->rawname, 16) == 0) {
	  puts("file exists");
	  return(NULL);
	}
      }
    }
    ts = next_ts_in_chain(di, ts);
  }

  /* allocate empty slot */
  ts = next_ts_in_chain(di, di->bam);
  while (ts.track) {
    buffer = get_ts_addr(di, ts);
    for (offset = 0; offset < 256; offset += 32) {
      rde = (RawDirEntry *)(buffer + offset);
      if (rde->type == 0) {
	memset((unsigned char *)rde + 2, 0, 30);
	memcpy(rde->rawname, rawname, 16);
	rde->type = type;
	return(rde);
      }
    }
    lastts = ts;
    ts = next_ts_in_chain(di, ts);
  }

  /* allocate new dir block */
  ts = alloc_next_dir_ts(di);
  if (ts.track) {
    rde = (RawDirEntry *)get_ts_addr(di, ts);
    memset((unsigned char *)rde + 2, 0, 30);
    memcpy(rde->rawname, rawname, 16);
    rde->type = type;
    return(rde);
  } else {
    puts("directory full");
    return(NULL);
  }
}


/* open a file */
ImageFile *di_open(DiskImage *di, unsigned char *rawname, FileType type, char *mode) {
  ImageFile *imgfile;
  RawDirEntry *rde;
  unsigned char *p;

  if (strcmp("rb", mode) == 0) {

    if ((imgfile = malloc(sizeof(*imgfile))) == NULL) {
      puts("malloc failed");
      return(NULL);
    }
    if (strcmp("$", rawname) == 0) {
      imgfile->mode = 'r';
      imgfile->ts = di->bam;
      imgfile->buffer = 2 + get_ts_addr(di, imgfile->ts);
      imgfile->buflen = 254;
      rde = NULL;
    } else {
      if ((rde = find_file_entry(di, rawname, type)) == NULL) {
	puts("find_file_entry failed");
	free(imgfile);
	return(NULL);
      }
      imgfile->mode = 'r';
      imgfile->ts = rde->startts;
      p = get_ts_addr(di, rde->startts);
      imgfile->buffer = p + 2;
      imgfile->nextts.track = p[0];
      imgfile->nextts.sector = p[1];
      if (imgfile->nextts.track == 0) {
	imgfile->buflen = imgfile->nextts.sector - 1;
      } else {
	imgfile->buflen = 254;
      }
    }

  } else if (strcmp("wb", mode) == 0) {

    if ((rde = alloc_file_entry(di, rawname, type)) == NULL) {
      puts("alloc_file_entry failed");
      return(NULL);
    }
    if ((imgfile = malloc(sizeof(*imgfile))) == NULL) {
      puts("malloc failed");
      return(NULL);
    }
    imgfile->mode = 'w';
    imgfile->ts.track = 0;
    imgfile->ts.sector = 0;
    if ((imgfile->buffer = malloc(254)) == NULL) {
      free(imgfile);
      puts("malloc failed");
      return(NULL);
    }
    imgfile->buflen = 254;
    di->modified = 1;

  } else {
    return(NULL);
  }

  imgfile->diskimage = di;
  imgfile->rawdirentry = rde;
  imgfile->position = 0;
  imgfile->bufptr = 0;

  ++(di->openfiles);
  return(imgfile);
}


int di_read(ImageFile *imgfile, unsigned char *buffer, int len) {
  unsigned char *p;
  int bytesleft;
  int counter = 0;

  while (len) {
    bytesleft = imgfile->buflen - imgfile->bufptr;
    if (bytesleft == 0) {
      if (imgfile->nextts.track == 0) {
	return(counter);
      }
      imgfile->ts = next_ts_in_chain(imgfile->diskimage, imgfile->ts);
      p = get_ts_addr(imgfile->diskimage, imgfile->ts);
      imgfile->buffer = p + 2;
      imgfile->nextts.track = p[0];
      imgfile->nextts.sector = p[1];
      if (imgfile->nextts.track == 0) {
	imgfile->buflen = imgfile->nextts.sector - 1;
      } else {
	imgfile->buflen = 254;
      }
      imgfile->bufptr = 0;
    } else {
      if (len >= bytesleft) {
	while (bytesleft) {
	  *buffer++ = imgfile->buffer[imgfile->bufptr++];
	  --len;
	  --bytesleft;
	  ++counter;
	  ++(imgfile->position);
	}
      } else {
	while (len) {
	  *buffer++ = imgfile->buffer[imgfile->bufptr++];
	  --len;
	  --bytesleft;
	  ++counter;
	  ++(imgfile->position);
	}
      }
    }
  }
  return(counter);
}


int di_write(ImageFile *imgfile, unsigned char *buffer, int len) {
  unsigned char *p;
  int bytesleft;
  int counter = 0;

  while (len) {
    bytesleft = imgfile->buflen - imgfile->bufptr;
    if (bytesleft == 0) {
      if (imgfile->diskimage->blocksfree == 0) {
	return(counter);
      }
      imgfile->nextts = alloc_next_ts(imgfile->diskimage, imgfile->ts);
      if (imgfile->ts.track == 0) {
	imgfile->rawdirentry->startts = imgfile->nextts;
      } else {
	p = get_ts_addr(imgfile->diskimage, imgfile->ts);
	p[0] = imgfile->nextts.track;
	p[1] = imgfile->nextts.sector;
      }
      imgfile->ts = imgfile->nextts;
      p = get_ts_addr(imgfile->diskimage, imgfile->ts);
      p[0] = 0;
      p[1] = 0xff;
      memcpy(p + 2, imgfile->buffer, 254);
      imgfile->bufptr = 0;
      if (++(imgfile->rawdirentry->sizelo) == 0) {
	++(imgfile->rawdirentry->sizehi);
      }
      --(imgfile->diskimage->blocksfree);
    } else {
      if (len >= bytesleft) {
	while (bytesleft) {
	  imgfile->buffer[imgfile->bufptr++] = *buffer++;
	  --len;
	  --bytesleft;
	  ++counter;
	  ++(imgfile->position);
	}
      } else {
	while (len) {
	  imgfile->buffer[imgfile->bufptr++] = *buffer++;
	  --len;
	  --bytesleft;
	  ++counter;
	  ++(imgfile->position);
	}
      }
    }
  }
  return(counter);
}


void di_close(ImageFile *imgfile) {
  unsigned char *p;

  if (imgfile->mode == 'w') {
    if (imgfile->bufptr) {
      if (imgfile->diskimage->blocksfree) {
	imgfile->nextts = alloc_next_ts(imgfile->diskimage, imgfile->ts);
	if (imgfile->ts.track == 0) {
	  imgfile->rawdirentry->startts = imgfile->nextts;
	} else {
	  p = get_ts_addr(imgfile->diskimage, imgfile->ts);
	  p[0] = imgfile->nextts.track;
	  p[1] = imgfile->nextts.sector;
	}
	imgfile->ts = imgfile->nextts;
	p = get_ts_addr(imgfile->diskimage, imgfile->ts);
	p[0] = 0;
	p[1] = 0xff;
	memcpy(p + 2, imgfile->buffer, 254);
	imgfile->bufptr = 0;
	if (++(imgfile->rawdirentry->sizelo) == 0) {
	  ++(imgfile->rawdirentry->sizehi);
	}
	--(imgfile->diskimage->blocksfree);
	imgfile->rawdirentry->type |= 0x80;
      }
    } else {
      imgfile->rawdirentry->type |= 0x80;
    }
    free(imgfile->buffer);
  }
  --(imgfile->diskimage->openfiles);
  free(imgfile);
}
