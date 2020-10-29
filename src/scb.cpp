#include "scb.h"

seginfo* ReassemblyQueue::freelist_ = NULL;
int ReassemblyQueue::n_scb_ = 0;

seginfo* ReassemblyQueue::newseginfo()
{
	seginfo *s;
	
	if( (s = freelist_) ){
		freelist_ = s->next_;
		return s;
	}else{
		return new seginfo;
	}
}

void ReassemblyQueue::deleteseginfo(seginfo* s)
{
    s->prev_ = NULL;
    s->startseq_ = 0;
    s->endseq_ = 0;
	s->next_ = freelist_;
	freelist_ = s;
}

ReassemblyQueue::~ReassemblyQueue()
{
    n_scb_ -= 1;
    if ( n_scb_ == 0 ) {
        seginfo *curr, *next;
        curr = freelist_;
        while (curr) {
            next = curr->next_;
            delete curr;
            curr = next;
        }
    }
}

/*
 * unlink a seginfo from its FIFO
 */
void
ReassemblyQueue::fremove(seginfo* p)
{
	if (p->prev_)
		p->prev_->next_ = p->next_;
	else
		head_ = p->next_;
	if (p->next_)
		p->next_->prev_ = p->prev_;
	else
		tail_ = p->prev_;
}

void
ReassemblyQueue::coalesce(seginfo* p, seginfo* n, seginfo* q) {
	if (p && q && p->endseq_ == n->startseq_ && n->endseq_ == q->startseq_) {
		// new block fills hole between p and q
		// delete the new block and the block after, update p
		fremove(n);
		fremove(q);
		p->endseq_ = q->endseq_;
		ReassemblyQueue::deleteseginfo(n);
		ReassemblyQueue::deleteseginfo(q);
	} else if (p && (p->endseq_ == n->startseq_)) {
		// new block joins p, but not q
		// update p with n's seq data, delete new block
		fremove(n);
		p->endseq_ = n->endseq_;
		ReassemblyQueue::deleteseginfo(n);
	} else if (q && (n->endseq_ == q->startseq_)) {
		// new block joins q, but not p
		// update q with n's seq data, delete new block
		fremove(n);
		q->startseq_ = n->startseq_;
		ReassemblyQueue::deleteseginfo(n);
		p = q;	// ensure p points to something
	}

	// at this point, p points to the updated/coalesced
	// block.  If it advances the highest in-seq value,
	// update rcv_next_ appropriately
	if (rcv_next_ >= p->startseq_)
		rcv_next_ = p->endseq_;
	return; 
}

void
ReassemblyQueue::add( int32_t start, int32_t end)
{
    bool needmerge = false;
    bool altered = false;

	if (end < start) {
		fprintf(stderr, "ReassemblyQueue::add() - end(%d) before start(%d)\n",
			end, start);
        return;
	}

	if (head_ == NULL) {
		// nobody there, just insert this one
		tail_ = head_ = ReassemblyQueue::newseginfo();
		head_->prev_ = head_->next_ = NULL;
		head_->startseq_ = start;
		head_->endseq_ = end;
		total_ = (end - start);

		// this shouldn't really happen, but
		// do the right thing just in case
        if (rcv_next_ >= start)
            rcv_next_ = end;
        return;

	} else {
again2:
		seginfo *p = NULL, *q = NULL, *n, *r;

		// in the code below, arrange for:
		// q: points to segment after this one
		// p: points to segment before this one
		if (start >= tail_->endseq_) {
			// at tail, no overlap
			p = tail_;
			if (start == tail_->endseq_)
				needmerge = true;
			goto endfast;
		}

		if (end <= head_->startseq_) {
			// at head, no overlap
			q = head_;
			if (end == head_->startseq_)
				needmerge = true;
			goto endfast;
		}

		// search for segments before and after
		// the new one; could be overlapped
		q = head_;
		while (q && q->startseq_ < end)
			q = q->next_;

		p = tail_;
		while (p && p->endseq_ > start)
			p = p->prev_;

		// kill anything that is completely overlapped
		r = p ? p : head_;
		while (r && r != q)  {
			if (start == r->startseq_ && end == r->endseq_) {
				// exact overlap
				return;
			} else if (start <= r->startseq_ && end >= r->endseq_) {
				// new segment completly overlap one segment
				total_ -= (r->endseq_ - r->startseq_);
				n = r;
				r = r->next_;
				fremove(n);
				ReassemblyQueue::deleteseginfo(n);
				altered = true;
			} else
				r = r->next_;
		}

		// if we completely overlapped everything, the list
		// will now be empty.  In this case, just add the new one
        if (empty())
            goto endfast;

        if (altered) {
            altered = false;
            goto again2;
        }

		// look for left-side merge
		// update existing seg's start seq with new start
		if (p == NULL || p->next_->startseq_ < start) {
			if (p == NULL)
				p = head_;
			else
				p = p->next_;
				
			if (start < p->startseq_) {
				total_ += (p->startseq_ - start);
				p->startseq_ = start;
			}
			start = p->endseq_;
			needmerge = true;
		}

		// look for right-side merge
		// update existing seg's end seq with new end
		if (q == NULL || q->prev_->endseq_ > end) {
			if (q == NULL)
				q = tail_;
			else
				q = q->prev_;

			if (end > q->endseq_) {
				total_ += (end - q->endseq_);
				q->endseq_ = end;
			}
			end = q->startseq_;
			needmerge = true;
		}

		if (end <= start) {
			if (rcv_next_ >= head_->startseq_)
				rcv_next_ = head_->endseq_;
			return;
		}

		// if p & q are adjacent and new one
		// fits between, that is an easy case
		if (!needmerge && p->next_ == q && p->endseq_ <= start && q->startseq_ >= end) {
			if (p->endseq_ == start || q->startseq_ == end)
				needmerge = true;
		}

endfast:
		n = ReassemblyQueue::newseginfo();
		n->startseq_ = start;
		n->endseq_ = end;

		n->prev_ = p;
		n->next_ = q;

		if (p)
			p->next_ = n;
		else
			head_ = n;

		if (q)
			q->prev_ = n;
		else
			tail_ = n;

		// If there is an adjacency condition,
		// call coalesce to deal with it.
		// If not, there is a chance we inserted
		// at the head at the rcv_next_ point.  In
		// this case we ned to update rcv_next_ to
		// the end of the newly-inserted segment
		total_ += (end - start);

		if (needmerge)
			return(coalesce(p, n, q));
		else if (rcv_next_ >= start) {
			rcv_next_ = end;
		}
        return;
    }
}

void ReassemblyQueue::clear()
{
    seginfo* p = head_;
    seginfo* q;
    while(p) {
        q = p->next_;
        ReassemblyQueue::deleteseginfo(p);
        p = q;
    }
    return;
}

// the segments will be cleared to only contain pkts
// whose seqs are bigger than param1(seq)
void ReassemblyQueue::clearto( int32_t seq )
{
	seginfo *p = head_, *q;
	while (p) {
		if (p->endseq_ <= seq) {
			q = p->next_;
			total_ -= (p->endseq_ - p->startseq_);
			fremove(p);
			ReassemblyQueue::deleteseginfo(p);
			p = q;
		} else
			break;
	}
	/* we might be trimming in the middle */
	if (p && p->startseq_ <= seq && p->endseq_ > seq) {
		total_ -= (seq - p->startseq_);
		p->startseq_ = seq;
	}
	return;
}

// return the next unsacked pkt whose seq is
// bigger than param1(seq)
int32_t ReassemblyQueue::nextHole( int32_t seq )
{
    seginfo* p;
    for (p = head_; p; p = p->next_) {
        if (p->startseq_ > seq) {
            return seq;
        }
        if ((p->startseq_ <= seq) && (p->endseq_ >= seq)) {
            return p->endseq_;
        }
    }
    return -1;
}

void ReassemblyQueue::dumpList()
{
    seginfo* p;
    for (p = head_; p; p = p->next_) {
        fprintf(stderr, "[%d, %d]\t", 
                p->startseq_, p->endseq_);
    }
}

#define SACK_LEFT(i) ( 2*i + 1 )
#define SACK_RIGHT(i) ( 2*i + 2 )

int32_t ScoreBoard::getNextRetran() {
    int32_t seq;
    if (seq_next_ < ack_cumu_)
        seq_next_ = ack_cumu_;
    seq = seq_next_;

    if ( (seq = rq_.nextHole(seq)) > 0 ) {
        if (seq > seq_next_)
            seq_next_ = seq;
    }
    if (seq < sack_high_)
        return seq;
    else
        return -1;
}

int ScoreBoard::update(int32_t last_ack, int32_t *sack_array) {
    if (ack_cumu_ < last_ack) {
        ack_cumu_ = last_ack;
    }
    if (sack_high_ < last_ack) {
        sack_high_ = last_ack;
    }
    if ( !rq_.empty() && rq_.rcv_next_ < last_ack) {
        rq_.rcv_next_ = last_ack;
        rq_.clearto( last_ack );
    }

    if (sack_array) {
        int32_t n_sack = sack_array[0];
        int32_t sack_left, sack_right;
        for (int sack_index = 0; sack_index < n_sack; ++sack_index) {
            sack_left = sack_array[SACK_LEFT(sack_index)];
            sack_right = sack_array[SACK_RIGHT(sack_index)];
            rq_.add(sack_left, sack_right);
            if (sack_high_ < sack_right) {
                sack_high_ = sack_right;
            }
        }
    }
    return 0;
}

void ScoreBoard::markRetran( int32_t retran_seq ) {
    if (retran_seq >= seq_next_)
        seq_next_ = retran_seq + 1;
}

void ScoreBoard::dumpBoard() {
    fprintf(stderr, "ack: %d, seq_next: %d, sack_high: %d, segs:",
            ack_cumu_,
            seq_next_,
            sack_high_);
    rq_.dumpList();
}

void ScoreBoard::clear() {
    ack_cumu_ = 0;
    sack_high_ = 0;
    seq_next_ = 0;
    rq_.clear();
}
