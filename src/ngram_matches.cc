#include <fuzzy/ngram_matches.hh>

#include <fuzzy/agenda_item.hh>

#include <cmath>
#include <cstdlib>

namespace fuzzy
{
  inline AgendaItem* NGramMatches::get_agendaitem(unsigned s_id) {
    auto it = _psentences.find(s_id);
    if (it != _psentences.end())
      return &it.value();
    return nullptr;
  }
  inline AgendaItem* NGramMatches::new_agendaitem(unsigned s_id, unsigned p_length) {
    auto it = _psentences.emplace(s_id, AgendaItem(s_id, p_length)).first;
    return &it.value();
  }

  unsigned compute_min_exact_match(float fuzzy, unsigned p_length)
  {
    const auto differences = (unsigned)std::ceil(p_length * (1.f - fuzzy));
    // we split (p_length - differences) in  (differences + 1) parts
    // the minimum value of the largest part size is obtained by dividing and taking ceil
    return std::ceil((p_length - differences) / (differences + 1.));
  }

  NGramMatches::NGramMatches(size_t size_tm,
                             float fuzzy, unsigned p_length,
                             unsigned min_seq_len,
                             const SuffixArray& suffixArray)
    /* add a small epsilon to avoid rounding errors counting for an error */
    : max_differences_with_pattern((unsigned)std::floor(p_length * (1.f - fuzzy) + 0.00005)),
      min_exact_match(compute_min_exact_match(fuzzy, p_length)),
      _p_length(p_length),
      _min_seq_len(min_seq_len),
      _suffixArray(suffixArray)
  {
    _psentences.reserve(p_length);
  }

  int
  NGramMatches::get_sentence_count() const
  {
    return _psentences.size();
  }

  tsl::hopscotch_map<unsigned, AgendaItem>&
  NGramMatches::get_psentences()
  {
    return _psentences;
  }

  void
  NGramMatches::register_ranges(Range range)
  {
    // lazy injection feature - if match_length smaller than min_seq_len, we will not process the suffixes for the moment
    if (range.match_length < min_exact_match || range.match_length < _min_seq_len)
      return;

    // For each suffix that matches at least r.match_length
    for (auto i = range.suffix_first; i < range.suffix_last; i++)
    {
      // The size difference between the suffix and the pattern is too large for the suffix to be accepted
      const auto sizeDifference = std::abs((long int)_p_length - (long int)_suffixArray.sentence_length(i));
      if (sizeDifference > max_differences_with_pattern)
        continue;

      // Get or create the AgendaItem corresponding to the sentence (of the suffix that matched)
      const auto sentence_id = _suffixArray.suffixid2sentenceid()[i].sentence_id;
      auto* agendaItem = get_agendaitem(sentence_id);
      if (!agendaItem)
        agendaItem = new_agendaitem(sentence_id, _p_length);

      // The match will update the AgendaItem entry only if its length is the longest to date.
      if (range.match_length <= static_cast<std::size_t>(agendaItem->maxmatch))
        continue;

      // Update the AgendaItem with the match
      for(size_t j=0; j < range.match_length; j++)
        if (!agendaItem->map_pattern[j])
        {
          agendaItem->map_pattern[j] = true;
          agendaItem->coverage++;
        }

      agendaItem->maxmatch = std::max<int>(agendaItem->maxmatch, range.match_length);
    }
  }
}
