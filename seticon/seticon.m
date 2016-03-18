#include <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>

void usage() {
	fprintf(stderr,
		"usage: seticon ICONFILE FILE\n"
		"\n"
		"set's the icon associated with FILE to contents of ICONFILE\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	if (argc!=3)
		usage();
	NSString *imagefilename = [NSString stringWithUTF8String:argv[1]];
	NSImage *image = [[NSImage alloc]
		initByReferencingFile:imagefilename];
	char fullpath[1024];
	char *err = realpath(argv[2], fullpath);
	NSString *file = [NSString stringWithUTF8String:fullpath];
	if (!image || !err)
		usage();
	BOOL ok = [[NSWorkspace sharedWorkspace]
		setIcon:image
		forFile:file
		options:0];
	if (ok == NO) {
		fprintf(stderr, "seticon: error: the icon at: %s was not set for: %s\n",
			argv[1], fullpath);
	}
}