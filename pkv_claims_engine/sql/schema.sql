CREATE TABLE IF NOT EXISTS subscribers (
    id    SERIAL PRIMARY KEY,
    name  TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS policies (
    id            SERIAL PRIMARY KEY,
    subscriber_id INTEGER NOT NULL REFERENCES subscribers(id),
    policy_number TEXT UNIQUE NOT NULL,
    start_date    DATE NOT NULL,
    end_date      DATE NOT NULL,
    annual_limit  NUMERIC(10,2) NOT NULL
);

CREATE TABLE IF NOT EXISTS coverages (
    id        SERIAL PRIMARY KEY,
    policy_id INTEGER NOT NULL REFERENCES policies(id) ON DELETE CASCADE,
    category  TEXT NOT NULL,
    rate      NUMERIC(5,4) NOT NULL CHECK (rate >= 0 AND rate <= 1),
    UNIQUE (policy_id, category)
);

CREATE TABLE IF NOT EXISTS claims (
    id             SERIAL PRIMARY KEY,
    policy_id      INTEGER NOT NULL REFERENCES policies(id),
    treatment_date DATE NOT NULL,
    category       TEXT NOT NULL,
    cost           NUMERIC(10,2) NOT NULL CHECK (cost > 0),
    deductible     NUMERIC(10,2) NOT NULL DEFAULT 0.00,
    status         TEXT NOT NULL DEFAULT 'pending'
                       CHECK (status IN ('pending', 'approved', 'rejected')),
    reimbursement  NUMERIC(10,2),
    created_at     TIMESTAMPTZ DEFAULT NOW()
);
