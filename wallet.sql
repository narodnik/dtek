CREATE TABLE wallet_table (
  idx int unique,
  public_point varchar(66) unique,
  private_key varchar(64),
  value int
)
