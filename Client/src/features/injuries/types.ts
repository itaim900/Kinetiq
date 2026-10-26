import type { BodyRegion, InjuryCondition, Muscle } from '../../api';

// Reference data the injuries screen loads once and passes down.
export interface Catalog {
  conditions: InjuryCondition[];
  regions: BodyRegion[];
  muscles: Muscle[];
}
