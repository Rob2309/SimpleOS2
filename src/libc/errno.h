#pragma once

extern __thread int errno;

#define EDOM -1
#define ERANGE -2
#define EILSEQ -3

#define EFILENOTFOUND -4
#define EPERMISSIONDENIED -5
#define EINVALIDFD -6
#define EINVALIDBUFFER -7
#define EINVALIDPATH -8
#define EINVALIDFS -9
#define EINVALIDDEV -10
#define EFILEEXISTS -11