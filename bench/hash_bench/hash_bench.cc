#include <iostream>
#include <list>
#include "time_stat.h"

//#include <libcuckoo/city_hasher.hh>
#include <libcuckoo/cuckoohash_map.hh>
#include <glib/glib.h>
#include "khash.h"
#include "uthash.h"
#include "Judy.h"

using namespace std;

KHASH_MAP_INIT_INT64(mu64, uint64_t);

struct u64_hash {
	uint64_t key;
	uint64_t value;
	UT_hash_handle hh;
};

#define N_KEYS (2 << 20)
///#define SEQ
#undef SEQ

int main(void)
{
	cuckoohash_map<uint64_t, uint64_t, std::hash<uint64_t>> Table;
	//cuckoohash_map<uint64_t, uint64_t, CityHasher<uint64_t>> Table;
	struct time_stats stats;
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<uint64_t> dist(0, N_KEYS);
	std::list<uint64_t> key_list;

	for (uint64_t i = 0 ; i < N_KEYS/4; i++) {
		key_list.push_back(dist(mt));
	}

	cout << "# of keys: " << N_KEYS << endl;
	time_stats_init(&stats, 1);
	//////// Cuckoo hashing
#if 1
#ifdef SEQ
	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (uint64_t i = 0 ; i < N_KEYS; i++)
		Table.insert(i, i * 2);

	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		uint64_t j;
		j = Table.find(i);

		if (j != (i*2)) {
			cout << "CUCKOO hash: value is wrong ";
			cout << i << " " << j << endl;
		}
	}

	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"CUCKOO HASH (SEQ) ---------------");
#else

	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (auto it: key_list) {
		Table.insert(it, it * 2);
	}

	for (auto it: key_list) {
		uint64_t j;
		j = Table.find(it);

		if (j != (it*2)) {
			cout << "CUCKOO hash: value is wrong ";
			cout << it << " " << j << endl;
		}
	}

	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"CUCKOO HASH (RAND) ---------------");
#endif
#endif

	//////// Glib hashing
#if 1
	GHashTable *_int64_hash;
	uint64_t *j;

	_int64_hash = g_hash_table_new(g_int64_hash, g_int64_equal); 

	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

#ifdef SEQ
	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		j = (uint64_t *)malloc(sizeof(uint64_t));
		*j = (i * 2);

		g_hash_table_insert(_int64_hash, &i, j);
	}

	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		//j = static_cast<uint64_t *>(g_hash_table_lookup(_int64_hash, &i));
		j = (uint64_t *)g_hash_table_lookup(_int64_hash, &i);

		if (*j != (i*2)) {
			cout << "Glib hash: value is wrong ";
			cout << i << " " << *j << endl;
		}
	}

	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"GLIB HASH (SEQ) ---------------");
#else

	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (auto it: key_list) {
		j = (uint64_t *)malloc(sizeof(uint64_t));
		*j = it * 2;
		g_hash_table_insert(_int64_hash, &it, j);
	}

	for (auto it: key_list) {
		j = (uint64_t *)g_hash_table_lookup(_int64_hash, &it);

		if (*j != (it*2)) {
			cout << "Glib hash: value is wrong ";
			cout << it << " " << *j << endl;
		}
	}

	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"GLIB HASH (RAND) ---------------");
#endif
#endif 

	//////// KLIB hashing
#if 1
	int ret;
	khiter_t k_iter;
	khash_t(mu64) *klib_ht = kh_init(mu64);

#ifdef SEQ
	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		k_iter = kh_put(mu64, klib_ht, i, &ret);
		if (ret == 0) 
			cout << "KLIB hash: Key exists" << endl;
		kh_value(klib_ht, k_iter) = (i * 2);
	}
	
	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		uint64_t value;
		k_iter = kh_put(mu64, klib_ht, i, &ret);
		value = kh_value(klib_ht, k_iter);

		if (value != (i * 2)) {
			cout << "KLIB hash: value is wrong ";
			cout << i << " " << value << endl;
		}
	}
	
	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"KLIB HASH (SEQ) ---------------");
#else
	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (auto it: key_list) {
		k_iter = kh_put(mu64, klib_ht, it, &ret);
		/*
		if (ret == 0) 
			cout << "KLIB hash: Key exists" << endl;
		*/
		kh_value(klib_ht, k_iter) = (it * 2);
	}
	
	for (auto it: key_list) {
		uint64_t value;
		k_iter = kh_put(mu64, klib_ht, it, &ret);
		value = kh_value(klib_ht, k_iter);

		if (value != (it * 2)) {
			cout << "KLIB hash: value is wrong ";
			cout << it << " " << value << endl;
			exit(-1);
		}
	}
	
	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"KLIB HASH (RAND) ---------------");
#endif
#endif

	//////// UT hashing
#if 1
	struct u64_hash *ut_hash = NULL;

#ifdef SEQ
	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		struct u64_hash *entry;
		entry = (struct u64_hash *)malloc(sizeof(struct u64_hash));

		entry->key = i;
		entry->value = i * 2;
		HASH_ADD(hh, ut_hash, key, sizeof(uint64_t), entry);
	}
	
	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		struct u64_hash *entry;

		HASH_FIND(hh, ut_hash, &i, sizeof(uint64_t), entry);
		if (!entry) 
			cout << "UT hash: cannot find entry" << endl;
		if (entry->value != (i * 2)) {
			cout << "UT hash: value is wrong ";
			cout << i << " " << entry->value << endl;
		}
	}
	
	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"UT HASH (SEQ) ---------------");
#else
	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (auto it : key_list) {
		struct u64_hash *entry;
		entry = (struct u64_hash *)malloc(sizeof(struct u64_hash));

		entry->key = it;
		entry->value = it * 2;
		HASH_ADD(hh, ut_hash, key, sizeof(uint64_t), entry);
	}
	
	for (auto it : key_list) {
		struct u64_hash *entry;

		HASH_FIND(hh, ut_hash, &it, sizeof(uint64_t), entry);
		if (!entry) 
			cout << "UT hash: cannot find entry" << endl;
		if (entry->value != (it * 2)) {
			cout << "UT hash: value is wrong ";
			cout << it << " " << entry->value << endl;
		}
	}
	
	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"UT HASH (RAND) ---------------");
#endif
#endif
	//////// Judy Array
	// http://judy.sourceforge.net/doc/JudyL_3x.htm
#if 1
	Word_t index, value, *p_value;
	Pvoid_t jd_array = NULL;
	assert(sizeof(uint64_t) == sizeof(Word_t));
#ifdef SEQ
	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		JLI(p_value, jd_array, i);
		*p_value = i * 2;
	}
	for (uint64_t i = 0 ; i < N_KEYS; i++) {
		JLG(p_value, jd_array, i);
		value = *p_value;

		if (value != (i * 2)) {
			cout << "Judy array: value is wrong ";
			cout << i << " " << value << endl;
		}
	}
	
	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"Judy array (SEQ) ---------------");
#else
	time_stats_reinit(&stats, 1);
	time_stats_start(&stats);

	for (auto it : key_list) {
		JLI(p_value, jd_array, it);
		*p_value = it * 2;
	}

	for (auto it : key_list) {
		JLG(p_value, jd_array, it);
		value = *p_value;

		if (value != (it * 2)) {
			cout << "Judy array: value is wrong ";
			cout << it << " " << value << endl;
		}
	}
	time_stats_stop(&stats);

	time_stats_print(&stats, (char *)"Judy array (Rand) ---------------");
#endif
#endif

	return 0;
}
