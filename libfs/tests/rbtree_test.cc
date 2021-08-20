#include <iostream>
#include <random>
#include <list>

#include "ds/rbtree.h"

using namespace std;

struct data_node {
	struct rb_node node;
	int key;
};

struct rb_root data_tree = RB_ROOT;

struct data_node *search(struct rb_root *root, int key)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct data_node *data = container_of(node, struct data_node, node);
		int result;

		result = key - data->key;

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

int insert(struct rb_root *root, struct data_node *data)
{
	struct rb_node **_new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*_new) {
		struct data_node *_this = container_of(*_new, struct data_node, node);
		int result = data->key - _this->key;

		parent = *_new;
		if (result < 0)
			_new = &((*_new)->rb_left);
		else if (result > 0)
			_new = &((*_new)->rb_right);
		else
			return EEXIST;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, _new);
	rb_insert_color(&data->node, root);

	return 0;
}

int main(void)
{
	random_device rd;
	mt19937 mt(rd());
	uniform_int_distribution<> dist(0, 10000);
	list<int> key_list;

	for (int i = 0; i < 20000; i++)
		key_list.push_back(dist(mt));

	for (auto it: key_list) {
		struct data_node *d;
		d = new struct data_node;
		d->key = it;
		insert(&data_tree, d);
	}

	for (struct rb_node *node = rb_first(&data_tree);
			node; node = rb_next(node)) {
		struct data_node *d = rb_entry(node, struct data_node, node);
		cout << d->key << endl;
		rb_erase(&d->node, &data_tree);
	}

	cout << "After erase keys" << endl;

	for (struct rb_node *node = rb_first(&data_tree);
			node; node = rb_next(node)) {
		struct data_node *d = rb_entry(node, struct data_node, node);
		cout << d->key << endl;
	}
}
