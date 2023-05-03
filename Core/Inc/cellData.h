#ifndef INC_CELLDATA_H_
#define INC_CELLDATA_H_

#define MAX_BRICK_WARNING_VOLTAGE   4.225f
#define MAX_BRICK_FAULT_VOLTAGE     4.25f
#define MAX_BRICK_VOLTAGE           4.2f
#define MIN_BRICK_WARNING_VOLTAGE   2.7f
#define MIN_BRICK_FAULT_VOLTAGE     2.5f
#define NOMINAL_BRICK_VOLTAGE       3.6f

#define MAX_BRICK_TEMP_WARNING_C    55.0f
#define MAX_BRICK_TEMP_FAULT_C      60.0f

#define MAH_TO_AH                   1.0f / 1000.0f
#define CELL_CAPACITY_MAH           3000.0f
#define CELL_CAPACITY_AH            CELL_CAPACITY_MAH * MAH_TO_AH
#define MAX_CHARGE_C_RATING         1


#endif /* INC_CELLDATA_H_ */
