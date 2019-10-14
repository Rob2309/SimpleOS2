#pragma once

namespace ktl {
	
	template<typename T>
	struct Anchor {
		T* next;
		T* prev;
	};

	template<typename T, Anchor<T> T::* AnchorMember>
	class AnchorList {
	public:
		class Iterator {
		public:
			friend class AnchorList;

		public:
			explicit Iterator(T* node)
				: m_Node(node)
			{ }

			Iterator& operator++() {
				m_Node = (m_Node->*AnchorMember).next;
				return *this;
			}
			Iterator& operator--() {
				m_Node = (m_Node->*AnchorMember).prev;
				return *this;
			}

			Iterator operator++(int) {
				T* tmp = m_Node;
				m_Node = (m_Node->*AnchorMember).next;
				return Iterator(tmp);
			}
			Iterator operator--(int) {
				T* tmp = m_Node;
				m_Node = (m_Node->*AnchorMember).prev;
				return Iterator(tmp);
			}

			T& operator* () {
				return *m_Node;
			}

			T* operator-> () {
				return m_Node;
			}

			bool operator== (const Iterator& r) const {
				return r.m_Node == m_Node;
			}
			bool operator!= (const Iterator& r) const {
				return !(*this == r);
			}

		private:
			T* m_Node;
		};

	public:
		AnchorList()
			: m_Head(nullptr), m_Tail(nullptr)
		{ }
		AnchorList(const AnchorList&) = delete;
		AnchorList(AnchorList&& r) = delete;

		AnchorList& operator= (const AnchorList&) = delete;
		AnchorList& operator= (AnchorList&&) = delete;

		void push_back(T* t) {
			(t->*AnchorMember).next = nullptr;
			(t->*AnchorMember).prev = m_Tail;
			if(m_Head == nullptr) {
				m_Head = m_Tail = t;
			} else {
				(m_Tail->*AnchorMember).next = t;
				m_Tail = t;
			}
		}
		void pop_back() {
			erase(before_end());
		}

		void push_front(T* t) {
			(t.*AnchorMember).next = m_Head;
			(t.*AnchorMember).prev = nullptr;
			if(m_Head == nullptr) {
				m_Head = m_Tail = t;
			} else {
				(m_Head->*AnchorMember).prev = t;
				m_Head = t;
			}
		}
		void pop_front() {
			erase(begin());
		}

		const T& back() const {
			return *m_Tail;
		}
		T& back() {
			return *m_Tail;
		}

		const T& front() const {
			return *m_Head;
		}
		T& front() {
			return *m_Head;
		}

		void erase(const Iterator& it) {
			Anchor<T>& anchor = it.m_Node->*AnchorMember;

			if(anchor.prev != nullptr) {
				(anchor.prev->*AnchorMember).next = anchor.next;
			} else {
				m_Head = anchor.next;
			}

			if(anchor.next != nullptr) {
				(anchor.next->*AnchorMember).prev = anchor.prev;
			} else {
				m_Tail = anchor.prev;
			}
		}
		void erase(const T* t) {
			for(auto a = begin(); a != end(); ++a) {
				if(a.m_Node == t) {
					erase(a);
					return;
				}
			}
		}

		bool empty() const {
			return m_Head == nullptr;
		}

		Iterator begin() {
			return Iterator(m_Head);
		}
		Iterator end() {
			return Iterator(nullptr);
		}
		Iterator before_end() {
			return Iterator(m_Tail);
		}

	private:
		T* m_Head;
		T* m_Tail;
	};
}