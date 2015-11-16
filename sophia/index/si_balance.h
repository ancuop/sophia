#ifndef SI_BALANCE_H_
#define SI_BALANCE_H_

/*
 * sophia database
 * sphia.org
 *
 * Copyright (c) Dmitry Simonenko
 * BSD License
*/

int si_branch(si*, sdc*, siplan*, uint64_t);
int si_compact(si*, sdc*, siplan*, uint64_t, ssiter*, uint32_t);
int si_compact_index(si*, sdc*, siplan*, uint64_t);

#endif
