/*
 * Usage: imageunzip <input file>
 *
 * Writes the uncompressed data to stdout.
 */

#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/time.h>

#define PTHREADS

#ifdef PTHREADS
#include <pthread.h>
#endif

#include "imagehdr.h"

#include "frisbee.h"

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

#define SECSIZE 512
#define BSIZE	(32 * 1024)
#define OUTSIZE (2 * BSIZE)
char		inbuf[BSIZE], outbuf[OUTSIZE + SECSIZE], zeros[BSIZE];

int		infd, outfd;
int		doseek = 0;
int		debug = 1;
long long	total = 0;
int		inflate_subblock(void);
void		writezeros(off_t zcount);

#ifdef linux
#define devlseek	lseek
#define devwrite	write
#else
static inline off_t devlseek(int fd, off_t off, int whence)
{
	off_t noff;
	assert((off & (SECSIZE-1)) == 0);
	noff = lseek(fd, off, whence);
	assert(noff == (off_t)-1 || (noff & (SECSIZE-1)) == 0);
	return noff;
}

static inline int devwrite(int fd, const void *buf, size_t size)
{
	assert((size & (SECSIZE-1)) == 0);
	return write(fd, buf, size);
}
#endif

/* frisbee */

static int frisbeeIndexIntoCurrentChunk = 0;
static int frisbee_done = 0;
static char * frisbee_data = 0;
static unsigned int ipaddr;
static const char * frisbee_addr;

void frisbeeEndChunk()
{
    frisbeeIndexIntoCurrentChunk = 0;
    frisbee_data = NULL;
}

int frisbeeRead( char * inbuf, int amt )
{
  /*  printf("FrisbeeRead begin blocking part..\n"); */

  while (!frisbee_data && !frisbee_done) { sleep(0); } /* wait for data */
  if (frisbee_done) {
    printf("frisbeeRead(): called, but done!");
    return 0;
  }

  /*printf("!"); fflush( NULL );*/
  /* printf("FrisbeeRead end blocking part..\n"); */
  assert(amt <= (SUBBLOCKSIZE - frisbeeIndexIntoCurrentChunk));
  memcpy( inbuf, frisbee_data + frisbeeIndexIntoCurrentChunk, amt ); 
  frisbeeIndexIntoCurrentChunk += amt; 
  return amt;
  /*
  if (frisbeeIndexIntoCurrentChunk == SUBBLOCKSIZE) {

  }
  */
}

void * frisbee_thread( void *arg );

void frisbeeBridgeInit( const char * address )
{
  frisbee_addr = address;
  frisbee_done = 0;
  frisbee_data = NULL;
  frisbeeIndexIntoCurrentChunk = 0;

#ifdef PTHREADS
  {
    pthread_t     frisbee_pid = 0;
    void	       *frisbee_thread(void *arg);
    /* int		frisbee_status; */

    printf("writeimage_driver: creating frisbee thread.\n");

    if (0 != pthread_create(&frisbee_pid, NULL,
		            /* &threadattr, */ &frisbee_thread, (void *) 0)) {
      perror("pthread_create failure.\n");
    } else {
      printf("pthread_create success!\n");
    }
  }
#endif

#ifdef RTHREADS
  if (-1 ==
      rfork_thread(RFPROC | RFMEM, 
                   malloc(655360), &frisbee_thread, (void *) 0 )) {
    perror("rfork_thread failure.\n");
  } else {
    printf("rfork_thread success!.\n");
  }
#endif
}

/* frisbee_thread.
 * this guy handles talking to frisbee.
 * its job is: when frisbee_data is null,
 * set it to point to new data if any is available.
 * Also set frisbee_done to 1 when all data has been received and used. */
void * frisbee_thread( void *arg )
{
  static unsigned int lastChunkId = 0xFFFFFFFF;
  static unsigned int doneGetting = 0; 

  printf("Frisbee starting, netaddr = 0x%08x\n", ipaddr ); 
  frisbeeInit2( "ignored.image", frisbee_addr );

  printf("frisbee_thread: entering mainloop 'while(!frisbee_done)'\n");
  while(!frisbee_done) {
    /*    pthread_testcancel(); */
    
    if (!doneGetting) {
      /* we're not done receiving, so service the protocol */
      /* hopefully this gets called a lot. */
      int err = frisbeeLoop();
      if (err != FRISBEE_OK) {
	if (err == FRISBEE_DONE) {
	  printf("frisbee_thread: frisbeeLoop() reports done.");
	} else {
	  printf("frisbee_thread: frisbeeLoop() returned funky value %i.\n", err);
	}
        /* weird error or actual DONE code. Either way, we're done receiving */
        printf("frisbee_thread: done getting.\n");
	doneGetting = 1;
      }
    }

    if (!frisbee_data) {
      /* either we havent given the consumer data yet, or they're done */
      if (lastChunkId != 0xFFFFFFFF) {
        /* they're done with a piece we gave them */
	frisbeeUnlockReadyChunk( lastChunkId );
	lastChunkId = 0xFFFFFFFF;
      }
      if (FRISBEE_NO_CHUNK == frisbeeLockReadyChunk( &lastChunkId, (u_char **)&frisbee_data )) {
	if (doneGetting) {
          printf("frisbee_thread: done getting and consuming.\n");
	  /* none available, and no more to come, so bye bye */
	  frisbee_done = 1;
	}
      } else {}
    }
  }

  frisbeeFinish();
  return 0;
}


/* end frisbee */

int
main(int argc, char **argv)
{
	struct timeval  stamp, estamp;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: "
		       "%s <network address> [output filename]\n", argv[0]);
		exit(1);
	}

        printf("calling frisbeebridgeinit...\n");
	frisbeeBridgeInit( argv[1] );
	/*
	if ((infd = open(argv[1], O_RDONLY, 0666)) < 0) {
		perror("opening input file");
		exit(1);
	}
	*/
	

        printf("opening outfile...\n");
	if (argc == 3) {
		if ((outfd =
		     open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0) {
			perror("opening output file");
			exit(1);
		}
		doseek = 1;
	}
	else
		outfd = fileno(stdout);
	
	printf("writing outfile, beginning mainloop.\n");

	gettimeofday(&stamp, 0);
	
	while (!frisbee_done) {
		off_t		offset, correction;
		
		/*
		 * Decompress one subblock at a time. After its done
		 * make sure the input file pointer is at a block
		 * boundry. Move it up if not. 
		 */
		if (inflate_subblock())
			break;
		/*
		if ((offset = lseek(infd, (off_t) 0, SEEK_CUR)) < 0) {
			perror("Getting current seek pointer");
			exit(1);
		}
		if (offset & (SUBBLOCKSIZE - 1)) {
			correction = SUBBLOCKSIZE -
				(offset & (SUBBLOCKSIZE - 1));

			if ((offset = lseek(infd, correction, SEEK_CUR)) < 0) {
				perror("correcting seek pointer");
				exit(1);
			}
		}
		*/
	}
	/* close(infd); */

	gettimeofday(&estamp, 0);
	estamp.tv_sec -= stamp.tv_sec;
	printf("Done in %ld seconds!\n", estamp.tv_sec);
	
	return 0;
}

int
inflate_subblock(void)
{
	int		cc, err, count, ibsize = 0, ibleft = 0;
	z_stream	d_stream; /* inflation stream */
	char		*bp;
	struct blockhdr *blockhdr;
	struct region	*curregion;
	off_t		offset, size;
	char		buf[DEFAULTREGIONSIZE];

	printf("Inflate subblock called...\n");

	if (frisbee_done) {
	  printf("But we're done!\n");
	  return 1;
	}
	d_stream.zalloc   = (alloc_func)0;
	d_stream.zfree    = (free_func)0;
	d_stream.opaque   = (voidpf)0;
	d_stream.next_in  = 0;
	d_stream.avail_in = 0;
	d_stream.next_out = 0;

	err = inflateInit(&d_stream);
	CHECK_ERR(err, "inflateInit");

	/*
	 * Read the header. It is uncompressed, and holds the real
	 * image size and the magic number.
	 */
	if ((cc = frisbeeRead( buf, DEFAULTREGIONSIZE)) <= 0) {
		if (cc == 0)
			return 1;
		perror("reading zipped image header goo");
		exit(1);
	}
	assert(cc == DEFAULTREGIONSIZE);
	blockhdr = (struct blockhdr *) buf;

	if (blockhdr->magic != COMPRESSED_MAGIC) {
		fprintf(stderr, "Bad Magic Number!\n");
		exit(1);
	}
	curregion = (struct region *) (blockhdr + 1);

	/*
	 * Start with the first region. 
	 */
	offset = curregion->start * (off_t) SECSIZE;
	size   = curregion->size  * (off_t) SECSIZE;
	assert(size);
	curregion++;
	blockhdr->regioncount--;

	if (debug)
		fprintf(stderr, "Decompressing: %14qd --> ", offset);

	while (1) {
		/*
		 * Read just up to the end of compressed data.
		 */
		if (blockhdr->size >= sizeof(inbuf))
			count = sizeof(inbuf);
		else
			count = blockhdr->size;
			
		if ((cc = frisbeeRead( inbuf, count)) <= 0) {
			if (cc == 0) {
				return 1;
			}
			perror("reading zipped image");
			exit(1);
		}
		assert(cc == count);
		blockhdr->size -= cc;

		d_stream.next_in   = inbuf;
		d_stream.avail_in  = cc;
	inflate_again:
		/*
		 * Must operate on multiples of the sector size!
		 */
		if (ibleft) {
			memcpy(outbuf, &outbuf[ibsize - ibleft], ibleft);
		}
		d_stream.next_out  = &outbuf[ibleft];
		d_stream.avail_out = OUTSIZE;

		err = inflate(&d_stream, Z_SYNC_FLUSH);
		if (err != Z_OK && err != Z_STREAM_END) {
			fprintf(stderr, "inflate failed, err=%ld\n", err);
			exit(1);
		}
		ibsize = (OUTSIZE - d_stream.avail_out) + ibleft;
		count  = ibsize & ~(SECSIZE - 1);
		ibleft = ibsize - count;
		bp     = outbuf;

		while (count) {
			/*
			 * Write data only as far as the end of the current
			 * region.
			 */
			if (count < size)
				cc = count;
			else
				cc = size;

			if ((cc = devwrite(outfd, bp, cc)) != cc) {
				if (cc < 0) {
					perror("Writing uncompressed data");
				}
				fprintf(stderr, "inflate failed\n");
				exit(1);
			}

			count  -= cc;
			bp     += cc;
			size   -= cc;
			offset += cc;
			total  += cc;

			/*
			 * Hit the end of the region. Need to figure out
			 * where the next one starts. We write a block of
			 * zeros in the empty space between this region
			 * and the next. We can lseek, but only if
			 * not writing to stdout. 
			 */
			if (! size) {
				off_t	newoffset;

				/*
				 * No more regions. Must be done.
				 */
				if (!blockhdr->regioncount)
					break;

				newoffset = curregion->start * (off_t) SECSIZE;

				writezeros(newoffset - offset);

				offset = newoffset;
				size   = curregion->size * (off_t) SECSIZE;
				assert(size);
				curregion++;
				blockhdr->regioncount--;
			}
		}
		if (d_stream.avail_in)
			goto inflate_again;

		if (err == Z_STREAM_END)
			break;
	}
	err = inflateEnd(&d_stream);
	CHECK_ERR(err, "inflateEnd");

	assert(blockhdr->regioncount == 0);
	assert(size == 0);
	assert(blockhdr->size == 0);

	if (debug)
		fprintf(stderr, "%14qd\n", total);

        frisbeeEndChunk();
	
	return 0;
}

void
writezeros(off_t zcount)
{
	int	zcc;

	if (doseek) {
		if (devlseek(outfd, zcount, SEEK_CUR) < 0) {
			perror("Skipping ahead");
			exit(1);
		}
		total  += zcount;
		return;
	}
	
	while (zcount) {
		if (zcount <= BSIZE)
			zcc = (int) zcount;
		else
			zcc = BSIZE;
		
		if ((zcc = devwrite(outfd, zeros, zcc)) != zcc) {
			if (zcc < 0) {
				perror("Writing Zeros");
			}
			exit(1);
		}
		zcount -= zcc;
		total  += zcc;
	}
}
