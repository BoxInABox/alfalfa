/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>
#include <string>
#include <tuple>
#include <limits>

#include "decoder.hh"
#include "frame.hh"
#include "frame_input.hh"
#include "vp8_raster.hh"
#include "ivf_writer.hh"
#include "costs.hh"
#include "enc_state_serializer.hh"
#include "file_descriptor.hh"

enum EncoderPass
{
  FIRST_PASS,
  SECOND_PASS
};

class Encoder
{
private:
  struct MBPredictionData
  {
    mbmode prediction_mode { DC_PRED };
    MotionVector mv {};

    uint32_t rate       { std::numeric_limits<uint32_t>::max() };
    uint32_t distortion { std::numeric_limits<uint32_t>::max() };
    uint32_t cost       { std::numeric_limits<uint32_t>::max() };
  };

  typedef SafeArray<SafeArray<std::pair<uint32_t, uint32_t>,
                              MV_PROB_CNT>,
                    2> MVComponentCounts;

  IVFWriter ivf_writer_;
  uint16_t width() const { return ivf_writer_.width(); }
  uint16_t height() const { return ivf_writer_.height(); }
  MutableRasterHandle temp_raster_handle_ { width(), height() };
  DecoderState decoder_state_;
  References references_;

  bool has_state_ { false };

  size_t qindex_ { 0 };

  Costs costs_ {};

  bool two_pass_encoder_ { false };

  // TODO: Where did these come from?
  uint32_t RATE_MULTIPLIER { 300 };
  uint32_t DISTORTION_MULTIPLIER { 1 };

  static uint32_t rdcost( uint32_t rate, uint32_t distortion,
                          uint32_t rate_multiplier,
                          uint32_t distortion_multiplier );

  template<unsigned int size>
  static int32_t avg( const VP8Raster::Block<size> & block,
                      const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t sad( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t sse( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction );

  MotionVector diamond_search( const VP8Raster::Macroblock & original_mb,
                               VP8Raster::Macroblock & reconstructed_mb,
                               VP8Raster::Macroblock & temp_mb,
                               InterFrameMacroblock & frame_mb,
                               const VP8Raster & reference,
                               MotionVector base_mv,
                               MotionVector origin,
                               size_t step_size ) const;

  void luma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              VP8Raster::Macroblock & temp_mb,
                              InterFrameMacroblock & frame_mb,
                              const Quantizer & quantizer,
                              MVComponentCounts & component_counts,
                              const EncoderPass encoder_pass = FIRST_PASS );

  void luma_mb_apply_inter_prediction( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       InterFrameMacroblock & frame_mb,
                                       const Quantizer & quantizer,
                                       const mbmode best_pred,
                                       const MotionVector best_mv,
                                       const EncoderPass encoder_pass );

  void chroma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                VP8Raster::Macroblock & temp_mb,
                                InterFrameMacroblock & frame_mb,
                                const Quantizer & quantizer,
                                const EncoderPass encoder_pass = FIRST_PASS ) const;

  template<class MacroblockType>
  MBPredictionData luma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                 VP8Raster::Macroblock & reconstructed_mb,
                                                 VP8Raster::Macroblock & temp_mb,
                                                 MacroblockType & frame_mb,
                                                 const Quantizer & quantizer,
                                                 const EncoderPass encoder_pass = FIRST_PASS,
                                                 const bool interframe = false ) const;

  template<class MacroblockType>
  void luma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       MacroblockType & frame_mb,
                                       const Quantizer & quantizer,
                                       const mbmode min_prediction_mode,
                                       const EncoderPass encoder_pass = FIRST_PASS ) const;

  template<class MacroblockType>
  void luma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              VP8Raster::Macroblock & temp_mb,
                              MacroblockType & frame_mb,
                              const Quantizer & quantizer,
                              const EncoderPass encoder_pass = FIRST_PASS ) const;

  MBPredictionData chroma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                   VP8Raster::Macroblock & reconstructed_mb,
                                                   VP8Raster::Macroblock & temp_mb,
                                                   const bool interframe = false ) const;

  template<class MacroblockType>
  void chroma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                         VP8Raster::Macroblock & reconstructed_mb,
                                         __attribute__((unused)) VP8Raster::Macroblock & temp_mb,
                                         MacroblockType & frame_mb,
                                         const Quantizer & quantizer,
                                         const mbmode min_prediction_mode,
                                         const EncoderPass encoder_pass ) const;


  template<class MacroblockType>
  void chroma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                VP8Raster::Macroblock & temp_mb,
                                MacroblockType & frame_mb,
                                const Quantizer & quantizer,
                                const EncoderPass encoder_pass = FIRST_PASS,
                                const bool interframe = false ) const;

  bmode luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                               VP8Raster::Block4 & constructed_sb,
                               VP8Raster::Block4 & temp_sb,
                               const SafeArray<uint16_t, num_intra_b_modes> & mode_costs ) const;

  template<class FrameType>
  std::pair<FrameType, double> encode_with_quantizer( const VP8Raster & raster,
                                                      const QuantIndices & quant_indices,
                                                      const bool update_state = false );

  template<class FrameType>
  double encode_raster( const VP8Raster & raster, const double minimum_ssim,
                        const uint8_t y_ac_qi = std::numeric_limits<uint8_t>::max() );

  template<class FrameSubblockType>
  void trellis_quantize( FrameSubblockType & frame_sb,
                         const Quantizer & quantizer ) const;

  void check_reset_y2( Y2Block & y2, const Quantizer & quantizer ) const;

  VP8Raster & temp_raster() { return temp_raster_handle_.get(); }

  template<class FrameType>
  void apply_best_loopfilter_settings( const VP8Raster & original,
                                       VP8Raster & reconstructed,
                                       FrameType & frame );

  template<class FrameType>
  void optimize_probability_tables( FrameType & frame, const TokenBranchCounts & token_branch_counts );

  template<class FrameHeaderType, class MacroblockType>
  void optimize_prob_skip( Frame<FrameHeaderType, MacroblockType> & frame );

  void optimize_interframe_probs( InterFrame & frame );
  void optimize_mv_probs( InterFrame & frame, const MVComponentCounts & counts );

  void update_mv_component_counts( const int16_t & component,
                                   const bool is_x,
                                   MVComponentCounts & counts ) const;

  static unsigned calc_prob( unsigned false_count, unsigned total );

  template<class FrameType>
  static FrameType make_empty_frame( const uint16_t width, const uint16_t height );

  template<class FrameType>
  void write_frame( const FrameType & frame );

  template<class FrameType>
  void write_frame( const FrameType & frame, const ProbabilityTables & prob_tables );

  /* Convergence-related stuff */
  template<class FrameType>
  InterFrame reencode_frame( const VP8Raster & unfiltered_output,
                       const FrameType & original_frame );

  InterFrame update_residues( const VP8Raster & unfiltered_output,
                              const InterFrame & original_frame );

  InterFrame create_switching_frame( const uint8_t y_ac_qi );

  void refine_switching_frame( InterFrame & frame,
                               const InterFrame & prev_frame,
                               const VP8Raster & d1 );

  void write_switching_frame( const InterFrame & frame );

  void fix_probability_tables( InterFrame & frame,
                               const ProbabilityTables & target );

public:
  Encoder( IVFWriter && output, const bool two_pass );

  Encoder( const Decoder & decoder, IVFWriter && output, const bool two_pass );

  double encode( const VP8Raster & raster,
                 const double minimum_ssim,
                 const uint8_t y_ac_qi = std::numeric_limits<uint8_t>::max() );

  void reencode( FrameInput & input, const IVF & pred_ivf, Decoder pred_decoder,
                 const uint8_t s_ac_qi, const bool refine_sw,
                 const bool fix_prob_tables );

  Decoder export_decoder() const { return { decoder_state_, references_ }; }
};

#endif /* ENCODER_HH */
