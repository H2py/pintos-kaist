/* 해시 테이블.

   이 자료 구조는 Pintos Project 3의 투어 문서에서 자세히 설명되어 있다.

   기본 정보는 hash.h를 참조하라.
*/

#include "hash.h"
#include "../debug.h"
#include "threads/malloc.h"

#define list_elem_to_hash_elem(LIST_ELEM)                       \
	list_entry(LIST_ELEM, struct hash_elem, list_elem)

static struct list *find_bucket (struct hash *, struct hash_elem *);
static struct hash_elem *find_elem (struct hash *, struct list *,
		struct hash_elem *);
static void insert_elem (struct hash *, struct list *, struct hash_elem *);
static void remove_elem (struct hash *, struct hash_elem *);
static void rehash (struct hash *);

/* 해시 테이블 H를 초기화한다.  
   주어진 보조 데이터 AUX를 바탕으로,  
   HASH 함수를 사용해 해시 값을 계산하고,  
   LESS 함수를 사용해 해시 요소들을 비교한다.
*/
bool
hash_init (struct hash *h,
		hash_hash_func *hash, hash_less_func *less, void *aux) {
	h->elem_cnt = 0;
	h->bucket_cnt = 4;
	h->buckets = malloc (sizeof *h->buckets * h->bucket_cnt);
	h->hash = hash;
	h->less = less;
	h->aux = aux;

	if (h->buckets != NULL) {
		hash_clear (h, NULL);
		return true;
	} else
		return false;
}

/* H 해시 테이블의 모든 요소를 제거한다.

   DESTRUCTOR가 null이 아니라면, 해시 테이블의 각 요소에 대해
   해당 함수가 호출된다. DESTRUCTOR는 필요하다면 해시 요소가
   사용하는 메모리를 해제할 수 있다.

   단, hash_clear()가 실행되는 동안에는, DESTRUCTOR 내부에서든
   외부에서든 다음 함수들(hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), hash_delete()) 중 어떤 것이든
   해시 테이블 H를 수정하는 동작은 정의되지 않은 동작을 유발한다.
*/
void
hash_clear (struct hash *h, hash_action_func *destructor) {
	size_t i;

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];

		if (destructor != NULL)
			while (!list_empty (bucket)) {
				struct list_elem *list_elem = list_pop_front (bucket);
				struct hash_elem *hash_elem = list_elem_to_hash_elem (list_elem);
				destructor (hash_elem, h->aux);
			}

		list_init (bucket);
	}

	h->elem_cnt = 0;
}

/* 해시 테이블 H를 파괴한다.

   DESTRUCTOR가 null이 아니라면, 먼저 해시 테이블의 각 요소에 대해
   해당 함수가 호출된다. DESTRUCTOR는 필요하다면 해시 요소가 사용하는
   메모리를 해제할 수 있다.

   단, hash_clear()가 실행되는 동안에는, DESTRUCTOR 내부에서든 외부에서든  
   hash_clear(), hash_destroy(), hash_insert(), hash_replace(), hash_delete()  
   중 어떤 함수라도 해시 테이블 H를 수정하는 행위는 정의되지 않은 동작을 유발한다.
*/
void
hash_destroy (struct hash *h, hash_action_func *destructor) {
	if (destructor != NULL)
		hash_clear (h, destructor);
	free (h->buckets);
}

/* NEW를 해시 테이블 H에 삽입하고,
   동일한 요소가 테이블에 존재하지 않으면 null 포인터를 반환한다.
   동일한 요소가 이미 테이블에 존재하는 경우에는,
   NEW는 삽입하지 않고 기존 요소를 반환한다.
*/
struct hash_elem *
hash_insert (struct hash *h, struct hash_elem *new) {
	struct list *bucket = find_bucket (h, new);
	struct hash_elem *old = find_elem (h, bucket, new);

	if (old == NULL)
		insert_elem (h, bucket, new);

	rehash (h);

	return old;
}

/* NEW를 해시 테이블 H에 삽입하며,  
   동일한 요소가 이미 테이블에 있다면 그것을 교체하고,  
   교체된 기존 요소를 반환한다.
*/
struct hash_elem *
hash_replace (struct hash *h, struct hash_elem *new) {
	struct list *bucket = find_bucket (h, new);
	struct hash_elem *old = find_elem (h, bucket, new);

	if (old != NULL)
		remove_elem (h, old);
	insert_elem (h, bucket, new);

	rehash (h);

	return old;
}

/* 해시 테이블 H에서 E와 동일한 요소를 찾아 반환한다.  
   동일한 요소가 존재하지 않으면 null 포인터를 반환한다.
*/
struct hash_elem *
hash_find (struct hash *h, struct hash_elem *e) {
	return find_elem (h, find_bucket (h, e), e);
}

/* 해시 테이블 H에서 E와 동일한 요소를 찾아 제거하고 반환한다.  
   동일한 요소가 존재하지 않으면 null 포인터를 반환한다.

   해시 테이블의 요소들이 동적으로 할당되었거나,  
   그런 자원을 소유하고 있다면,  
   해당 자원을 해제하는 책임은 호출자에게 있다.
*/
struct hash_elem *
hash_delete (struct hash *h, struct hash_elem *e) {
	struct hash_elem *found = find_elem (h, find_bucket (h, e), e);
	if (found != NULL) {
		remove_elem (h, found);
		rehash (h);
	}
	return found;
}

/* 해시 테이블 H의 각 요소에 대해, 임의의 순서로 ACTION을 호출한다.

   hash_apply()가 실행되는 동안,
   hash_clear(), hash_destroy(), hash_insert(), hash_replace(), hash_delete()
   중 어느 함수라도 해시 테이블 H를 수정하면 정의되지 않은 동작(UB)을 유발한다.
   이는 ACTION 내부에서든, 외부에서든 마찬가지다.
*/
void
hash_apply (struct hash *h, hash_action_func *action) {
	size_t i;

	ASSERT (action != NULL);

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];
		struct list_elem *elem, *next;

		for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) {
			next = list_next (elem);
			action (list_elem_to_hash_elem (elem), h->aux);
		}
	}
}

/* I를 해시 테이블 H의 반복(iteration)을 위해 초기화한다.

   반복(iteration) 사용 예시:

   struct hash_iterator i;

   hash_first(&i, h);
   while (hash_next(&i))
   {
     struct foo *f = hash_entry(hash_cur(&i), struct foo, elem);
     ... f를 사용해 어떤 작업을 수행 ...
   }

   반복(iteration) 도중에 hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), hash_delete() 등의 함수를 사용하여
   해시 테이블 H를 수정하면 모든 반복자(iterator)가 무효화된다.*/
void
hash_first (struct hash_iterator *i, struct hash *h) {
	ASSERT (i != NULL);
	ASSERT (h != NULL);

	i->hash = h;
	i->bucket = i->hash->buckets;
	i->elem = list_elem_to_hash_elem (list_head (i->bucket));
}

/* I를 해시 테이블에서 다음 요소로 이동시키고,  
   해당 요소를 반환한다.  
   더 이상 남은 요소가 없다면 null 포인터를 반환한다.  
   요소들은 임의의 순서로 반환된다.

   반복(iteration) 중에 hash_clear(), hash_destroy(),  
   hash_insert(), hash_replace(), hash_delete() 등의  
   함수로 해시 테이블 H를 수정하면, 모든 반복자가 무효화된다.
*/
struct hash_elem *
hash_next (struct hash_iterator *i) {
	ASSERT (i != NULL);

	i->elem = list_elem_to_hash_elem (list_next (&i->elem->list_elem));
	while (i->elem == list_elem_to_hash_elem (list_end (i->bucket))) {
		if (++i->bucket >= i->hash->buckets + i->hash->bucket_cnt) {
			i->elem = NULL;
			break;
		}
		i->elem = list_elem_to_hash_elem (list_begin (i->bucket));
	}

	return i->elem;
}

/* 해시 테이블 순회 중 현재 요소를 반환한다.  
   테이블의 끝에 도달한 경우에는 null 포인터를 반환한다.

   단, hash_first()를 호출한 직후 hash_next()를 호출하기 전에  
   hash_cur()를 사용하면 정의되지 않은 동작이 발생한다.
*/
struct hash_elem *
hash_cur (struct hash_iterator *i) {
	return i->elem;
}

/* H에 있는 요소(원소)의 개수를 반환한다. */
size_t
hash_size (struct hash *h) {
	return h->elem_cnt;
}

/* H가 아무 요소도 포함하지 않으면 true를 반환하고,  
   하나라도 요소가 있으면 false를 반환한다. */
bool
hash_empty (struct hash *h) {
	return h->elem_cnt == 0;
}

/* Fowler-Noll-Vo 해시(FNV 해시) 상수들,  
   32비트 워드 크기를 위한 것이다. */
#define FNV_64_PRIME 0x00000100000001B3UL
#define FNV_64_BASIS 0xcbf29ce484222325UL

/* BUF에 있는 SIZE 바이트 데이터를 해싱하여  
   해시 값을 반환한다. */
uint64_t
hash_bytes (const void *buf_, size_t size) {
	/* Fowler-Noll-Vo 32-bit hash, for bytes. */
	const unsigned char *buf = buf_;
	uint64_t hash;

	ASSERT (buf != NULL);

	hash = FNV_64_BASIS;
	while (size-- > 0)
		hash = (hash * FNV_64_PRIME) ^ *buf++;

	return hash;
}

/* 문자열 S의 해시 값을 반환한다. */
uint64_t
hash_string (const char *s_) {
	const unsigned char *s = (const unsigned char *) s_;
	uint64_t hash;

	ASSERT (s != NULL);

	hash = FNV_64_BASIS;
	while (*s != '\0')
		hash = (hash * FNV_64_PRIME) ^ *s++;

	return hash;
}

/* 정수 I의 해시 값을 반환한다. */
uint64_t
hash_int (int i) {
	return hash_bytes (&i, sizeof i);
}

/* 해시 테이블 H에서 요소 E가 속한 버킷을 반환한다. */
static struct list *
find_bucket (struct hash *h, struct hash_elem *e) {
	size_t bucket_idx = h->hash (e, h->aux) & (h->bucket_cnt - 1);
	return &h->buckets[bucket_idx];
}

/* 해시 테이블 H의 BUCKET에서 요소 E와 같은 해시 요소를 검색한다.
   찾으면 해당 요소를 반환하고, 없으면 null 포인터를 반환한다. */
static struct hash_elem *
find_elem (struct hash *h, struct list *bucket, struct hash_elem *e) {
	struct list_elem *i;

	for (i = list_begin (bucket); i != list_end (bucket); i = list_next (i)) {
		struct hash_elem *hi = list_elem_to_hash_elem (i);
		if (!h->less (hi, e, h->aux) && !h->less (e, hi, h->aux))
			return hi;
	}
	return NULL;
}

/* X의 최하위 비트 중 1로 설정된 비트를 0으로 바꾼 값을 반환한다. */
static inline size_t
turn_off_least_1bit (size_t x) {
	return x & (x - 1);
}

/* X가 2의 거듭제곱이면 true를 반환하고, 그렇지 않으면 false를 반환한다. */
static inline size_t
is_power_of_2 (size_t x) {
	return x != 0 && turn_off_least_1bit (x) == 0;
}

/* 버킷당 요소(원소)의 비율 관련 값들. */
#define MIN_ELEMS_PER_BUCKET  1 /* 버킷당 요소 수가 1보다 작으면: 버킷 수를 줄인다. */
#define BEST_ELEMS_PER_BUCKET 2 /* 이상적인 버킷당 요소 수. */
#define MAX_ELEMS_PER_BUCKET  4 /* 버킷당 요소 수가 4보다 크면: 버킷 수를 늘린다. */

/* 해시 테이블 H의 버킷 수를 이상적인 값에 맞게 변경한다.
   이 함수는 메모리 부족(out-of-memory) 상황 때문에 실패할 수 있지만,
   그럴 경우 해시 접근이 다소 비효율적일 뿐이며, 프로그램은 계속 진행할 수 있다. */
static void
rehash (struct hash *h) {
	size_t old_bucket_cnt, new_bucket_cnt;
	struct list *new_buckets, *old_buckets;
	size_t i;

	ASSERT (h != NULL);

	/* 나중에 사용할 수 있도록 기존 버킷 정보를 저장한다. */
	old_buckets = h->buckets;
	old_bucket_cnt = h->bucket_cnt;

	/*지금 사용할 버킷 수를 계산한다.
	BEST_ELEMS_PER_BUCKET마다 하나의 버킷이 있도록 설정하고자 한다.
	최소한 4개의 버킷은 있어야 하며, 버킷의 수는 반드시 2의 제곱수여야 한다*/
	new_bucket_cnt = h->elem_cnt / BEST_ELEMS_PER_BUCKET;
	if (new_bucket_cnt < 4)
		new_bucket_cnt = 4;
	while (!is_power_of_2 (new_bucket_cnt))
		new_bucket_cnt = turn_off_least_1bit (new_bucket_cnt);

	/* 버킷 수가 바뀌지 않는다면 아무것도 하지 마라.*/
	if (new_bucket_cnt == old_bucket_cnt)
		return;

	/* 새로운 버킷 배열을 할당하고, 각 버킷을 비어 있는 상태로 초기화하라 */
	new_buckets = malloc (sizeof *new_buckets * new_bucket_cnt);
	if (new_buckets == NULL) {
		/* 메모리 할당에 실패했다.
		이는 해시 테이블의 사용이 덜 효율적이게 됨을 의미한다.
		그러나 여전히 사용 가능하므로, 이것을 오류로 처리할 이유는 없다.  */
		return;
	}
	for (i = 0; i < new_bucket_cnt; i++)
		list_init (&new_buckets[i]);

	/* 새로운 버킷 정보를 설치한다.  */
	h->buckets = new_buckets;
	h->bucket_cnt = new_bucket_cnt;

	/* 각 기존 요소를 적절한 새 버킷으로 옮긴다. */
	for (i = 0; i < old_bucket_cnt; i++) {
		struct list *old_bucket;
		struct list_elem *elem, *next;

		old_bucket = &old_buckets[i];
		for (elem = list_begin (old_bucket);
				elem != list_end (old_bucket); elem = next) {
			struct list *new_bucket
				= find_bucket (h, list_elem_to_hash_elem (elem));
			next = list_next (elem);
			list_remove (elem);
			list_push_front (new_bucket, elem);
		}
	}

	free (old_buckets);
}

/* 해시 테이블 H의 BUCKET에 E를 삽입한다. */
static void
insert_elem (struct hash *h, struct list *bucket, struct hash_elem *e) {
	h->elem_cnt++;
	list_push_front (bucket, &e->list_elem);
}

/* 해시 테이블 H에서 E를 제거한다.*/
static void
remove_elem (struct hash *h, struct hash_elem *e) {
	h->elem_cnt--;
	list_remove (&e->list_elem);
}

