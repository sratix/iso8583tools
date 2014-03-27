#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "parser.h"

int main(int argc, char **argv)
{
	fldformat *frmtmp, *frm=NULL;
	field *message;
	char format_dir[]="./formats";
	char filename[sizeof(format_dir)+NAME_MAX+1];
	DIR *frmdir;
	struct dirent *de;
	int frmcounter=0;
	char msgbuf[1000];
	char msgbuf2[1000];
	unsigned int msglen=0;
	unsigned int msglen2=0;
	FILE *infile=stdin, *outfile;

	if(argc>2)
	{
		printf("Usage %s [filename]\nfilename is the file to read the message from. If omitted, read from standard input\n", argv[0]);
		return 1;
	}

	frmdir=opendir(format_dir);

	if(!frmdir)
	{
		printf("Error: Can't open %s: %s\n", format_dir, strerror(errno));
		return 2;
	}

	while(de=readdir(frmdir))
	{
		#ifdef _DIRENT_HAVE_D_TYPE
			if(de->d_type!=DT_LNK && de->d_type!=DT_REG && de->d_type!=DT_UNKNOWN)
				continue;
		#endif

		if(strcmp(de->d_name+strlen(de->d_name)-4, ".frm"))
			continue;

		sprintf(filename, "%s/%s", format_dir, de->d_name);

		printf("Loading %s\n", filename);

		frmtmp=load_format(filename, frm);

		if(!frmtmp)
			continue;

		if(frmcounter++==0)
			frm=frmtmp;

		if(frmtmp->lengthFormat!=FRM_UNKNOWN)
		{
			if(load_format(filename, frm))		//load again without length as an altformat
			{
				frmcounter++;
				frmtmp->lengthFormat=FRM_UNKNOWN;
				frmtmp->lengthLength=0;
			}
		}
	}

	closedir(frmdir);

	if(frmcounter==0)
	{
		printf("Error: No formats loaded\n");
		return 3;
	}

	printf("Info: Loaded %d formats\n", frmcounter);

	if(argc>1)
	{
		infile=fopen(argv[1], "r");
		if(!infile)
		{
			printf("Error: Cannot open file %s: %s\n", argv[1], strerror(errno));
			return 4;
		}

		printf("Reading from %s\n", argv[1]);
	}
	else
		printf("Reading from stdin\n");

	while ((msgbuf[msglen++]=fgetc(infile))!=EOF)
		if(msglen>sizeof(msgbuf))
		{
			printf("Message is too big\n");
			if(infile!=stdin)
				fclose(infile);
			freeFormat(frm);
			return 5;
		}

	if(infile!=stdin)
		fclose(infile);

	msglen--;

	message=parse_message(msgbuf, msglen, frm);
	if(!message)
	{
		printf("Error: Unable to parse message\n");
		freeFormat(frm);
		return 6;
	}

	printf("%s parsed\n", message->frm->description);

	print_message(message);

	printf("Building %s, estimated length: %d\n", message->frm->description, get_length(message));

	msglen2=build_message(msgbuf2, sizeof(msgbuf2), message);

	if(!msglen2)
	{
		printf("Error: Unable to build %s\n", message->frm->description);
		freeField(message);
		freeFormat(frm);
		return 7;
	}

	printf("%s built. Length: %d\n", message->frm->description, msglen2);

	freeField(message);
	freeFormat(frm);

	if(msglen2!=msglen)
		printf("Warning: Total length don't match (%d != %d)\n", msglen, msglen2);
	else
		for(msglen=0; msglen<msglen2; msglen++)
			if(msgbuf[msglen]!=msgbuf2[msglen])
			{
				printf("Warning: Messages don't match (starting from byte %d)\n", msglen);
				break;
			}

	if(msglen2==msglen)
		printf("Rebuilt message match original\n");
	else
	{
		outfile=fopen("message_out", "w");
		for(msglen=0; msglen<msglen2; fputc(msgbuf2[msglen++], outfile));
		fclose(outfile);
		printf("message_out file is written\n");

		return 8;
	}

	return 0;
}

