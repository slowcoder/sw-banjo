#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "shared/util.h"

  /** Generates a 64bit UUID.
   * Loops though all NICs and uses their MAC to generate a
   * repeatable UUID.
   * @returns 0 on failure, UUID on success
   */
uint64_t util_getUUID(void) {
	struct ifreq  ifs[32];
	struct ifconf ifc;
	int count;
	int fd;
	int rc;
	int i;
	uint64_t uuid = 0;
	int idx = 0;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		return -1;

	ifc.ifc_len = sizeof(ifs);
	ifc.ifc_req = ifs;

	rc = ioctl(fd, SIOCGIFCONF, &ifc);
	if (rc < 0) {
		close(fd);
		return -1;
	}

	count = ifc.ifc_len / sizeof(ifs[0]);
	for (i = 0; i < count; ++i) {
		unsigned char *p;
		int j;

		if (ifs[i].ifr_addr.sa_family != AF_INET)
			continue;

		rc = ioctl(fd, SIOCGIFHWADDR, &ifs[i]);
		if (rc < 0)
			continue;

		p = (unsigned char *)ifs[i].ifr_hwaddr.sa_data;
		for (j = 0; j < 6; ++j) {
			uuid ^= (p[j]<<((idx%8)*8));
			idx++;
		}
	}
	close(fd);

	return uuid;
}

void util_gethostname(char *pzStr,int len) {
	gethostname(pzStr, len);
}
