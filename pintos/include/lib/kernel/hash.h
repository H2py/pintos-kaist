#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* 해시 테이블.

 *
 * 이 자료구조는 Project 3의 Pintos 투어에서 자세히 설명되어 있다.
 *
 * 이것은 체이닝(chaining)을 사용하는 표준적인 해시 테이블이다.
 * 테이블에서 요소를 찾기 위해, 요소의 데이터에 해시 함수를 적용하고
 * 그 값을 배열의 인덱스로 사용하여 이중 연결 리스트(doubly linked list)에 접근한 후,
 * 리스트를 선형 탐색(linearly search)한다.
 *
 * 체인 리스트들은 동적 할당을 사용하지 않는다.
 * 대신, 해시에 들어갈 가능성이 있는 구조체들은
 * 반드시 `struct hash_elem` 멤버를 포함해야 한다.
 * 모든 해시 함수는 이 `struct hash_elem`에 대해 동작한다.
 * `hash_entry` 매크로는 `struct hash_elem`에서 다시
 * 그것을 포함하고 있는 구조체 객체로 변환해준다.
 * 이 기법은 연결 리스트 구현에서도 사용된다.
 * 자세한 설명은 lib/kernel/list.h를 참조하라.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* 해시 요소. */
struct hash_elem {
	struct list_elem list_elem;
};

/* 해시 요소 포인터인 HASH_ELEM을,
 * HASH_ELEM이 포함된 바깥쪽 구조체의 포인터로 변환한다.
 * 바깥 구조체의 이름 STRUCT와,
 * 그 안에 포함된 해시 요소 멤버의 이름 MEMBER를 인자로 제공해야 한다.
 * 예시는 이 파일 상단의 큰 주석을 참조하라.
 */
#define hash_entry(HASH_ELEM, STRUCT, MEMBER)                   \
	((STRUCT *) ((uint8_t *) &(HASH_ELEM)->list_elem        \
		- offsetof (STRUCT, MEMBER.list_elem)))

/* 해시 요소 E에 대해,
 * 보조 데이터 AUX를 이용하여 해시 값을 계산하고 반환한다.
 */
typedef uint64_t hash_hash_func (const struct hash_elem *e, void *aux);

/* 두 해시 요소 A와 B의 값을,
 * 보조 데이터 AUX를 이용하여 비교한다.
 * A가 B보다 작으면 true를 반환하고,
 * A가 B보다 크거나 같으면 false를 반환한다.
 */
typedef bool hash_less_func (const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux);

/* 해시 요소 E에 대해,
 * 보조 데이터 AUX를 사용하여 어떤 연산을 수행한다.
 */
typedef void hash_action_func (struct hash_elem *e, void *aux);

/* 해시 테이블. */
struct hash {
	size_t elem_cnt;            /* 테이블에 있는 요소의 개수. */
	size_t bucket_cnt;          /* 버킷의 개수, 2의 거듭제곱이다. */
	struct list *buckets;       /* `bucket_cnt` 개수만큼의 리스트 배열. */
	hash_hash_func *hash;       /* 해시 함수. */
	hash_less_func *less;       /* 비교 함수. */
	void *aux;                  /* `hash`와 `less` 함수에 사용되는 보조 데이터. */
};

/* 해시 테이블 반복자. */
struct hash_iterator {
	struct hash *hash;          /* 해시 테이블. */
	struct list *bucket;        /* 현재 버킷. */
	struct hash_elem *elem;    /* 현재 버킷 안에서의 현재 해시 요소. */
};

/* 기본 생명 주기. */
bool hash_init (struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear (struct hash *, hash_action_func *);
void hash_destroy (struct hash *, hash_action_func *);

/* 탐색, 삽입, 삭제. */
struct hash_elem *hash_insert (struct hash *, struct hash_elem *);
struct hash_elem *hash_replace (struct hash *, struct hash_elem *);
struct hash_elem *hash_find (struct hash *, struct hash_elem *);
struct hash_elem *hash_delete (struct hash *, struct hash_elem *);

/* 반복(순회). */
void hash_apply (struct hash *, hash_action_func *);
void hash_first (struct hash_iterator *, struct hash *);
struct hash_elem *hash_next (struct hash_iterator *);
struct hash_elem *hash_cur (struct hash_iterator *);

/* 정보. */
size_t hash_size (struct hash *);
bool hash_empty (struct hash *);

/* 샘플 해시 함수들. */
uint64_t hash_bytes (const void *, size_t);
uint64_t hash_string (const char *);
uint64_t hash_int (int);

unsigned hash_func (const struct hash_elem *p_, void *aux);
bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);


#endif /* lib/kernel/hash.h */
